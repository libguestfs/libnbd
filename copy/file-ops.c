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

static size_t
file_synch_read (struct rw *rw,
                 void *data, size_t len, uint64_t offset)
{
  size_t n = 0;
  ssize_t r;

  assert (rw->t == LOCAL);

  while (len > 0) {
    r = pread (rw->u.local.fd, data, len, offset);
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
  ssize_t r;

  assert (rw->t == LOCAL);

  while (len > 0) {
    r = pwrite (rw->u.local.fd, data, len, offset);
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
  assert (rw->t == LOCAL);

#ifdef FALLOC_FL_PUNCH_HOLE
  int fd = rw->u.local.fd;
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
  assert (rw->t == LOCAL);

  if (S_ISREG (rw->u.local.stat.st_mode)) {
#ifdef FALLOC_FL_ZERO_RANGE
    int fd = rw->u.local.fd;
    int r;

    r = fallocate (fd, FALLOC_FL_ZERO_RANGE, offset, count);
    if (r == -1) {
      perror ("fallocate: FALLOC_FL_ZERO_RANGE");
      exit (EXIT_FAILURE);
    }
    return true;
#endif
  }
  else if (S_ISBLK (rw->u.local.stat.st_mode) &&
           IS_ALIGNED (offset | count, rw->u.local.sector_size)) {
#ifdef BLKZEROOUT
    int fd = rw->u.local.fd;
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
                  struct buffer *buffer,
                  nbd_completion_callback cb)
{
  assert (rw->t == LOCAL);

  file_synch_read (rw, buffer->data, buffer->len, buffer->offset);
  errno = 0;
  if (cb.callback (cb.user_data, &errno) == -1) {
    perror (rw->name);
    exit (EXIT_FAILURE);
  }
}

static void
file_asynch_write (struct rw *rw,
                   struct buffer *buffer,
                   nbd_completion_callback cb)
{
  assert (rw->t == LOCAL);

  file_synch_write (rw, buffer->data, buffer->len, buffer->offset);
  errno = 0;
  if (cb.callback (cb.user_data, &errno) == -1) {
    perror (rw->name);
    exit (EXIT_FAILURE);
  }
}

static bool
file_asynch_trim (struct rw *rw, struct buffer *buffer,
                  nbd_completion_callback cb)
{
  assert (rw->t == LOCAL);

  if (!file_synch_trim (rw, buffer->offset, buffer->len))
    return false;
  errno = 0;
  if (cb.callback (cb.user_data, &errno) == -1) {
    perror (rw->name);
    exit (EXIT_FAILURE);
  }
  return true;
}

static bool
file_asynch_zero (struct rw *rw, struct buffer *buffer,
                  nbd_completion_callback cb)
{
  assert (rw->t == LOCAL);

  if (!file_synch_zero (rw, buffer->offset, buffer->len))
    return false;
  errno = 0;
  if (cb.callback (cb.user_data, &errno) == -1) {
    perror (rw->name);
    exit (EXIT_FAILURE);
  }
  return true;
}

static void
file_get_extents (struct rw *rw, uintptr_t index,
                  uint64_t offset, uint64_t count,
                  extent_list *ret)
{
  assert (rw->t == LOCAL);

  ret->size = 0;

#ifdef SEEK_HOLE
  static pthread_mutex_t lseek_lock = PTHREAD_MUTEX_INITIALIZER;

  if (rw->u.local.seek_hole_supported) {
    uint64_t end = offset + count;
    int fd = rw->u.local.fd;
    off_t pos;
    struct extent e;

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
        e.hole = true;
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
        e.hole = false;
        if (extent_list_append (ret, e) == -1) {
          perror ("realloc");
          exit (EXIT_FAILURE);
        }
      }

      offset = pos;
    } while (offset < end);

    pthread_mutex_unlock (&lseek_lock);
    return;
  }
#endif

  /* Otherwise return the default extent covering the whole range. */
  default_get_extents (rw, index, offset, count, ret);
}


struct rw_ops file_ops = {
  .synch_read = file_synch_read,
  .synch_write = file_synch_write,
  .synch_trim = file_synch_trim,
  .synch_zero = file_synch_zero,
  .asynch_read = file_asynch_read,
  .asynch_write = file_asynch_write,
  .asynch_trim = file_asynch_trim,
  .asynch_zero = file_asynch_zero,
  .get_extents = file_get_extents,
};
