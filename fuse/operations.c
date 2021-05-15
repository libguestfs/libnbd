/* NBD client library in userspace
 * Copyright (C) 2013-2021 Red Hat Inc.
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

/* FUSE operations invoked by the kernel.
 *
 * Note these may be called in parallel from multiple threads, so any
 * shared state needs to be read-only or else protected by mutexes.
 * libnbd calls are OK.
 */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <getopt.h>
#include <limits.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pthread.h>

#include <libnbd.h>

#include "nbdfuse.h"
#include "minmax.h"

#define MAX_REQUEST_SIZE (32 * 1024 * 1024)

/* Number of seconds to wait for commands to complete when closing the file. */
#define RELEASE_TIMEOUT 5

/* This operations background thread runs while nbdfuse is running and
 * is responsible for dispatching AIO commands.
 *
 * The commands themselves are initiated by the FUSE threads (by
 * calling eg. nbd_aio_pread), and then those threads call
 * wait_for_completion() which waits for the command to retire.
 *
 * A condition variable is signalled by any FUSE thread when it has
 * started a new AIO command and wants the operations thread to start
 * processing (if it isn't doing so already).  To signal completion we
 * use a completion callback which signals a per-thread completion
 * condition.
 */
static void *operations_thread (void *);

void
start_operations_thread (void)
{
  int err;
  pthread_t t;

  err = pthread_create (&t, NULL, operations_thread, NULL);
  if (err != 0) {
    errno = err;
    perror ("nbdfuse: pthread_create");
    exit (EXIT_FAILURE);
  }
}

static pthread_mutex_t start_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t start_cond = PTHREAD_COND_INITIALIZER;

struct completion {
  pthread_mutex_t mutex;
  pthread_cond_t cond;
  bool completed;
} completion;

static void *
operations_thread (void *arg)
{
  while (1) {
    /* Sleep until a command is in flight. */
    pthread_mutex_lock (&start_mutex);
    while (nbd_aio_in_flight (nbd.ptr[0]) == 0)
      pthread_cond_wait (&start_cond, &start_mutex);
    pthread_mutex_unlock (&start_mutex);

    /* Dispatch work while there are commands in flight. */
    while (nbd_aio_in_flight (nbd.ptr[0]) > 0)
      nbd_poll (nbd.ptr[0], -1);
  }

  /*NOTREACHED*/
  return NULL;
}

/* Completion callback - called from the operations thread when a
 * command completes.
 */
static int
completion_callback (void *vp, int *error)
{
  struct completion *completion = vp;

  pthread_mutex_lock (&completion->mutex);
  /* Mark the command as completed. */
  completion->completed = true;
  pthread_cond_signal (&completion->cond);
  pthread_mutex_unlock (&completion->mutex);

  /* Don't retire the command.  We want to get the error indication in
   * the FUSE thread.
   */
  return 0;
}

/* Report an NBD error and return -errno. */
static int
report_nbd_error (void)
{
  int err;

  fprintf (stderr, "%s\n", nbd_get_error ());
  err = nbd_get_errno ();
  if (err != 0)
    return -err;
  else
    return -EIO;
}

static int
wait_for_completion (struct completion *completion, int64_t cookie)
{
  int r;

  /* Signal to the operations thread to start work, in case it is sleeping. */
  pthread_mutex_lock (&start_mutex);
  pthread_cond_signal (&start_cond);
  pthread_mutex_unlock (&start_mutex);

  /* Wait until the completion_callback sets the completed flag.
   *
   * We cannot call nbd_aio_command_completed yet because that can
   * lead to a possible deadlock where completion_callback holds the
   * NBD handle lock and we try to acquire it by calling
   * nbd_aio_command_completed.  That is the reason for the
   * completion.completed flag.
   */
  pthread_mutex_lock (&completion->mutex);
  while (!completion->completed)
    pthread_cond_wait (&completion->cond, &completion->mutex);
  pthread_mutex_unlock (&completion->mutex);

  /* nbd_aio_command_completed returns:
   *  0 => command still in flight (should be impossible)
   *  1 => completed successfully
   * -1 => error
   */
  r = nbd_aio_command_completed (nbd.ptr[0], cookie);
  assert (r != 0);
  return r;
}

/* Wrap calls to any asynch command and check the error. */
#define CHECK_NBD_ASYNC_ERROR(CALL)                                     \
  do {                                                                  \
    struct completion completion =                                      \
      { PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER, false };   \
    nbd_completion_callback cb =                                        \
      { .callback = completion_callback, .user_data = &completion };    \
    int64_t cookie = (CALL);                                            \
    if (cookie == -1 || wait_for_completion (&completion, cookie) == -1) \
      return report_nbd_error ();                                       \
  } while (0)

/* Wraps calls to sync libnbd functions and check the error. */
#define CHECK_NBD_SYNC_ERROR(CALL)                                      \
  do { if ((CALL) == -1) return report_nbd_error (); } while (0)

static int
nbdfuse_getattr (const char *path, struct stat *statbuf,
                 struct fuse_file_info *fi)
{
  const int mode = readonly ? 0444 : 0666;

  memset (statbuf, 0, sizeof *statbuf);

  /* We're probably making some Linux-specific assumptions here, but
   * this file is not usually compiled on non-Linux systems (perhaps
   * on OpenBSD?).  XXX
   */
  statbuf->st_atim = start_t;
  statbuf->st_mtim = start_t;
  statbuf->st_ctim = start_t;
  statbuf->st_uid = geteuid ();
  statbuf->st_gid = getegid ();

  if (file_mode || (path[0] == '/' && strcmp (path+1, filename) == 0)) {
    /* getattr "/filename" */
    statbuf->st_mode = S_IFREG | mode;
    statbuf->st_nlink = 1;
    statbuf->st_size = size;
  }
  else if (strcmp (path, "/") == 0) {
    /* getattr "/" */
    statbuf->st_mode = S_IFDIR | (mode & 0111);
    statbuf->st_nlink = 2;
  }
  else
    return -ENOENT;

  return 0;
}

static int
nbdfuse_readdir (const char *path, void *buf,
                 fuse_fill_dir_t filler,
                 off_t offset, struct fuse_file_info *fi,
                 enum fuse_readdir_flags flags)
{
  if (strcmp (path, "/") != 0)
    return -ENOENT;

  filler (buf, ".", NULL, 0, 0);
  filler (buf, "..", NULL, 0, 0);
  filler (buf, filename, NULL, 0, 0);

  return 0;
}

/* This function checks the O_RDONLY/O_RDWR flags passed to the
 * open(2) call, so we have to check the open mode is compatible with
 * the readonly flag.
 */
static int
nbdfuse_open (const char *path, struct fuse_file_info *fi)
{
  if (!file_mode && (path[0] != '/' || strcmp (path+1, filename) != 0))
    return -ENOENT;

  if (readonly && (fi->flags & O_ACCMODE) != O_RDONLY)
    return -EACCES;

  return 0;
}

static int
nbdfuse_read (const char *path, char *buf,
              size_t count, off_t offset,
              struct fuse_file_info *fi)
{
  if (!file_mode && (path[0] != '/' || strcmp (path+1, filename) != 0))
    return -ENOENT;

  if (offset >= size)
    return 0;

  if (count > MAX_REQUEST_SIZE)
    count = MAX_REQUEST_SIZE;

  if (offset + count > size)
    count = size - offset;

  CHECK_NBD_ASYNC_ERROR (nbd_aio_pread (nbd.ptr[0], buf, count, offset, cb, 0));

  return (int) count;
}

static int
nbdfuse_write (const char *path, const char *buf,
               size_t count, off_t offset,
               struct fuse_file_info *fi)
{
  /* Probably shouldn't happen because of nbdfuse_open check. */
  if (readonly)
    return -EACCES;

  if (!file_mode && (path[0] != '/' || strcmp (path+1, filename) != 0))
    return -ENOENT;

  if (offset >= size)
    return 0;

  if (count > MAX_REQUEST_SIZE)
    count = MAX_REQUEST_SIZE;

  if (offset + count > size)
    count = size - offset;

  CHECK_NBD_ASYNC_ERROR (nbd_aio_pwrite (nbd.ptr[0], buf, count, offset, cb, 0));

  return (int) count;
}

static int
nbdfuse_fsync (const char *path, int datasync, struct fuse_file_info *fi)
{
  if (readonly)
    return 0;

  /* If the server doesn't support flush then the operation is
   * silently ignored.
   */
  if (nbd_can_flush (nbd.ptr[0]))
    CHECK_NBD_ASYNC_ERROR (nbd_aio_flush (nbd.ptr[0], cb, 0));

  return 0;
}

/* This is called on the last close of a file. */
static int
nbdfuse_release (const char *path, struct fuse_file_info *fi)
{
  time_t st;

  /* We do a synchronous flush here to be on the safe side, but it's
   * not strictly necessary.
   */
  if (!readonly && nbd_can_flush (nbd.ptr[0]))
    CHECK_NBD_SYNC_ERROR (nbd_flush (nbd.ptr[0], 0));

  /* Wait until there are no more commands in flight or until a
   * timeout is reached.
   */
  time (&st);
  while (1) {
    if (nbd_aio_in_flight (nbd.ptr[0]) == 0)
      break;
    if (time (NULL) - st > RELEASE_TIMEOUT)
      break;

    /* Signal to the operations thread to work. */
    pthread_mutex_lock (&start_mutex);
    pthread_cond_signal (&start_cond);
    pthread_mutex_unlock (&start_mutex);
  }

 return 0;
}

/* Punch a hole or write zeros. */
static int
nbdfuse_fallocate (const char *path, int mode, off_t offset, off_t len,
                   struct fuse_file_info *fi)
{
  if (readonly)
    return -EACCES;

  if (mode & FALLOC_FL_PUNCH_HOLE) {
    if (!nbd_can_trim (nbd.ptr[0]))
      return -EOPNOTSUPP;       /* Trim not supported. */
    else {
      CHECK_NBD_ASYNC_ERROR (nbd_aio_trim (nbd.ptr[0], len, offset, cb, 0));
      return 0;
    }
  }
  /* As of FUSE 35 this is not supported by the kernel module and it
   * always returns EOPNOTSUPP.
   */
  else if (mode & FALLOC_FL_ZERO_RANGE) {
    /* If the backend doesn't support writing zeroes then we can
     * emulate it.
     */
    if (!nbd_can_zero (nbd.ptr[0])) {
      static char zerobuf[4096];

      while (len > 0) {
        off_t n = MIN (len, sizeof zerobuf);
        CHECK_NBD_ASYNC_ERROR (nbd_aio_pwrite (nbd.ptr[0], zerobuf, n, offset,
                                               cb, 0));
        len -= n;
      }
      return 0;
    }
    else {
      CHECK_NBD_ASYNC_ERROR (nbd_aio_zero (nbd.ptr[0], len, offset, cb, 0));
      return 0;
    }
  }
  else
    return -EOPNOTSUPP;
}

struct fuse_operations nbdfuse_operations = {
  .getattr           = nbdfuse_getattr,
  .readdir           = nbdfuse_readdir,
  .open              = nbdfuse_open,
  .read              = nbdfuse_read,
  .write             = nbdfuse_write,
  .fsync             = nbdfuse_fsync,
  .release           = nbdfuse_release,
  .fallocate         = nbdfuse_fallocate,
};
