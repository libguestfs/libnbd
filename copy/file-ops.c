/* NBD client library in userspace.
 * Copyright (C) 2020 Red Hat Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/types.h>

#include <pthread.h>

#if defined (__linux__)
#include <linux/fs.h>       /* For BLKZEROOUT */
#endif

#include "isaligned.h"
#include "nbdcopy.h"

static struct rw_ops file_ops;

struct rw_file {
  struct rw rw;
  int fd;
  struct stat stat;
  bool seek_hole_supported;
  int sector_size;
};

static bool
seek_hole_supported (int fd)
{
#ifndef SEEK_HOLE
  return false;
#else
  off_t r = lseek (fd, 0, SEEK_HOLE);
  return r >= 0;
#endif
}

struct rw *
file_create (const char *name, const struct stat *stat, int fd)
{
  struct rw_file *rwf = calloc (1, sizeof *rwf);
  if (rwf == NULL) { perror ("calloc"); exit (EXIT_FAILURE); }

  rwf->rw.ops = &file_ops;
  rwf->rw.name = name;
  rwf->stat = *stat;
  rwf->fd = fd;

  if (S_ISBLK (stat->st_mode)) {
    /* Block device. */
    rwf->rw.size = lseek (fd, 0, SEEK_END);
    if (rwf->rw.size == -1) {
      perror ("lseek");
      exit (EXIT_FAILURE);
    }
    if (lseek (fd, 0, SEEK_SET) == -1) {
      perror ("lseek");
      exit (EXIT_FAILURE);
    }
    rwf->seek_hole_supported = seek_hole_supported (fd);
    rwf->sector_size = 4096;
#ifdef BLKSSZGET
    if (ioctl (fd, BLKSSZGET, &rwf->sector_size))
      fprintf (stderr, "warning: cannot get sector size: %s: %m", name);
#endif
  }
  else if (S_ISREG (stat->st_mode)) {
    /* Regular file. */
    rwf->rw.size = stat->st_size;
    rwf->seek_hole_supported = seek_hole_supported (fd);
  }
  else
    abort ();

  return &rwf->rw;
}

static void
file_close (struct rw *rw)
{
  struct rw_file *rwf = (struct rw_file *)rw;

  if (close (rwf->fd) == -1) {
    fprintf (stderr, "%s: close: %m\n", rw->name);
    exit (EXIT_FAILURE);
  }
  free (rw);
}

static void
file_truncate (struct rw *rw, int64_t size)
{
  struct rw_file *rwf = (struct rw_file *) rw;

  /* If the destination is an ordinary file then the original file
   * size doesn't matter.  Truncate it to the source size.  But
   * truncate it to zero first so the file is completely empty and
   * sparse.
   */
  if (! S_ISREG (rwf->stat.st_mode))
    return;

  if (ftruncate (rwf->fd, 0) == -1 ||
      ftruncate (rwf->fd, size) == -1) {
    perror ("truncate");
    exit (EXIT_FAILURE);
  }
  rwf->rw.size = size;

  /* We can assume the destination is zero. */
  destination_is_zero = true;
}

static void
file_flush (struct rw *rw)
{
  struct rw_file *rwf = (struct rw_file *)rw;

  if ((S_ISREG (rwf->stat.st_mode) ||
       S_ISBLK (rwf->stat.st_mode)) &&
      fsync (rwf->fd) == -1) {
    perror (rw->name);
    exit (EXIT_FAILURE);
  }
}

static bool
file_is_read_only (struct rw *rw)
{
  /* Permissions are hard, and this is only used as an early check
   * before the copy.  Proceed with the copy and fail if it fails.
   */
  return false;
}

static bool
file_can_extents (struct rw *rw)
{
#ifdef SEEK_HOLE
  return true;
#else
  return false;
#endif
}

static bool
file_can_multi_conn (struct rw *rw)
{
  return true;
}

static void
file_start_multi_conn (struct rw *rw)
{
  /* Don't need to do anything for files since we can read/write on a
   * single file descriptor.
   */
}

static size_t
file_synch_read (struct rw *rw,
                 void *data, size_t len, uint64_t offset)
{
  struct rw_file *rwf = (struct rw_file *)rw;
  size_t n = 0;
  ssize_t r;

  while (len > 0) {
    r = pread (rwf->fd, data, len, offset);
    if (r == -1) {
      perror (rw->name);
      exit (EXIT_FAILURE);
    }
    if (r == 0)
      return n;

    data += r;
    offset += r;
    len -= r;
    n += r;
  }

  return n;
}

static void
file_synch_write (struct rw *rw,
                  const void *data, size_t len, uint64_t offset)
{
  struct rw_file *rwf = (struct rw_file *)rw;
  ssize_t r;

  while (len > 0) {
    r = pwrite (rwf->fd, data, len, offset);
    if (r == -1) {
      perror (rw->name);
      exit (EXIT_FAILURE);
    }
    data += r;
    offset += r;
    len -= r;
  }
}

static bool
file_synch_trim (struct rw *rw, uint64_t offset, uint64_t count)
{
#ifdef FALLOC_FL_PUNCH_HOLE
  struct rw_file *rwf = (struct rw_file *)rw;
  int fd = rwf->fd;
  int r;

  r = fallocate (fd, FALLOC_FL_PUNCH_HOLE | FALLOC_FL_KEEP_SIZE,
                 offset, count);
  if (r == -1) {
    perror ("fallocate: FALLOC_FL_PUNCH_HOLE");
    exit (EXIT_FAILURE);
  }
  return true;
#else /* !FALLOC_FL_PUNCH_HOLE */
  return false;
#endif
}

static bool
file_synch_zero (struct rw *rw, uint64_t offset, uint64_t count)
{
  struct rw_file *rwf = (struct rw_file *)rw;

  if (S_ISREG (rwf->stat.st_mode)) {
#ifdef FALLOC_FL_ZERO_RANGE
    int fd = rwf->fd;
    int r;

    r = fallocate (fd, FALLOC_FL_ZERO_RANGE, offset, count);
    if (r == -1) {
      perror ("fallocate: FALLOC_FL_ZERO_RANGE");
      exit (EXIT_FAILURE);
    }
    return true;
#endif
  }
  else if (S_ISBLK (rwf->stat.st_mode) &&
           IS_ALIGNED (offset | count, rwf->sector_size)) {
#ifdef BLKZEROOUT
    int fd = rwf->fd;
    int r;
    uint64_t range[2] = {offset, count};

    r = ioctl (fd, BLKZEROOUT, &range);
    if (r == -1) {
      perror ("ioctl: BLKZEROOUT");
      exit (EXIT_FAILURE);
    }
    return true;
#endif
  }

  return false;
}

static void
file_asynch_read (struct rw *rw,
                  struct command *command,
                  nbd_completion_callback cb)
{
  file_synch_read (rw, slice_ptr (command->slice),
                   command->slice.len, command->offset);
  errno = 0;
  if (cb.callback (cb.user_data, &errno) == -1) {
    perror (rw->name);
    exit (EXIT_FAILURE);
  }
}

static void
file_asynch_write (struct rw *rw,
                   struct command *command,
                   nbd_completion_callback cb)
{
  file_synch_write (rw, slice_ptr (command->slice),
                    command->slice.len, command->offset);
  errno = 0;
  if (cb.callback (cb.user_data, &errno) == -1) {
    perror (rw->name);
    exit (EXIT_FAILURE);
  }
}

static bool
file_asynch_trim (struct rw *rw, struct command *command,
                  nbd_completion_callback cb)
{
  if (!file_synch_trim (rw, command->offset, command->slice.len))
    return false;
  errno = 0;
  if (cb.callback (cb.user_data, &errno) == -1) {
    perror (rw->name);
    exit (EXIT_FAILURE);
  }
  return true;
}

static bool
file_asynch_zero (struct rw *rw, struct command *command,
                  nbd_completion_callback cb)
{
  if (!file_synch_zero (rw, command->offset, command->slice.len))
    return false;
  errno = 0;
  if (cb.callback (cb.user_data, &errno) == -1) {
    perror (rw->name);
    exit (EXIT_FAILURE);
  }
  return true;
}

static unsigned
file_in_flight (struct rw *rw, uintptr_t index)
{
  return 0;
}

static void
file_get_extents (struct rw *rw, uintptr_t index,
                  uint64_t offset, uint64_t count,
                  extent_list *ret)
{
  ret->size = 0;

#ifdef SEEK_HOLE
  struct rw_file *rwf = (struct rw_file *)rw;
  static pthread_mutex_t lseek_lock = PTHREAD_MUTEX_INITIALIZER;

  if (rwf->seek_hole_supported) {
    uint64_t end = offset + count;
    int fd = rwf->fd;
    off_t pos;
    struct extent e;
    size_t last;

    pthread_mutex_lock (&lseek_lock);

    /* This loop is taken pretty much verbatim from nbdkit-file-plugin. */
    do {
      pos = lseek (fd, offset, SEEK_DATA);
      if (pos == -1) {
        if (errno == ENXIO)
          pos = end;
        else {
          perror ("lseek: SEEK_DATA");
          exit (EXIT_FAILURE);
        }
      }

      /* We know there is a hole from offset to pos-1. */
      if (pos > offset) {
        e.offset = offset;
        e.length = pos - offset;
        e.zero = true;
        if (extent_list_append (ret, e) == -1) {
          perror ("realloc");
          exit (EXIT_FAILURE);
        }
      }

      offset = pos;
      if (offset >= end)
        break;

      pos = lseek (fd, offset, SEEK_HOLE);
      if (pos == -1) {
        perror ("lseek: SEEK_HOLE");
        exit (EXIT_FAILURE);
      }

      /* We know there is allocated data from offset to pos-1. */
      if (pos > offset) {
        e.offset = offset;
        e.length = pos - offset;
        e.zero = false;
        if (extent_list_append (ret, e) == -1) {
          perror ("realloc");
          exit (EXIT_FAILURE);
        }
      }

      offset = pos;
    } while (offset < end);

    /* The last extent may extend beyond the request bounds.  We must
     * truncate it.
     */
    assert (ret->size > 0);
    last = ret->size - 1;
    assert (ret->ptr[last].offset <= end);
    if (ret->ptr[last].offset + ret->ptr[last].length > end) {
      uint64_t d = ret->ptr[last].offset + ret->ptr[last].length - end;
      ret->ptr[last].length -= d;
      assert (ret->ptr[last].offset + ret->ptr[last].length == end);
    }

    pthread_mutex_unlock (&lseek_lock);
    return;
  }
#endif

  /* Otherwise return the default extent covering the whole range. */
  default_get_extents (rw, index, offset, count, ret);
}

static struct rw_ops file_ops = {
  .ops_name = "file_ops",
  .close = file_close,
  .is_read_only = file_is_read_only,
  .can_extents = file_can_extents,
  .can_multi_conn = file_can_multi_conn,
  .start_multi_conn = file_start_multi_conn,
  .truncate = file_truncate,
  .flush = file_flush,
  .synch_read = file_synch_read,
  .synch_write = file_synch_write,
  .synch_trim = file_synch_trim,
  .synch_zero = file_synch_zero,
  .asynch_read = file_asynch_read,
  .asynch_write = file_asynch_write,
  .asynch_trim = file_asynch_trim,
  .asynch_zero = file_asynch_zero,
  .in_flight = file_in_flight,
  .get_polling_fd = get_polling_fd_not_supported,
  .asynch_notify_read = asynch_notify_read_write_not_supported,
  .asynch_notify_write = asynch_notify_read_write_not_supported,
  .get_extents = file_get_extents,
};
