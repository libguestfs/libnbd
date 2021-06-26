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

#ifdef HAVE_STDATOMIC_H
#include <stdatomic.h>
#else
/* Rely on ints being atomic enough on the platform. */
#define _Atomic /**/
#endif

#include <libnbd.h>

#include "nbdfuse.h"
#include "minmax.h"

#define MAX_REQUEST_SIZE (32 * 1024 * 1024)

/* Number of seconds to wait for commands to complete when closing the file. */
#define RELEASE_TIMEOUT 5

#define DEBUG_OPERATION(name, fs, ...)                                  \
  do {                                                                  \
    if (verbose)                                                        \
      fprintf (stderr, "nbdfuse: %s: " fs "\n", name, ##__VA_ARGS__);   \
  } while (0)

/* NOTES ON THE THREAD MODEL
 *
 * Once nbdfuse is up and running there will be some number of FUSE
 * threads (controlled by fuse itself, see -o max_idle_threads) and a
 * static number of operations background threads.  Under load there
 * should be more FUSE threads than operations threads -- things work
 * more efficiently when this is the case.
 *
 * The NBD commands are initiated by the FUSE threads (eg. by calling
 * functions like nbd_aio_pread).  The FUSE thread then calls
 * wait_for_completion() which blocks the FUSE thread waiting for the
 * command to retire.
 *
 * Commands are distributed to operations threads (currently round-
 * robin, but we could be smarter about this).
 *
 * An operation thread handles a single NBD connection but possibly
 * multiple commands in flight.  It is a loop which waits on a
 * condition for at least one command to be in flight, then loops
 * calling nbd_poll while there are commands in flight, then goes back
 * to waiting on the condition.
 */

static void *operations_thread (void *);

static struct thread {
  size_t n;
  pthread_t thread;

  /* This counts the number of commands in flight.  The condition is
   * used to allow the operations thread to process commands when
   * in_flight goes from 0 -> 1.  This is roughly equivalent to
   * nbd_aio_in_flight, but we need to count it ourselves in order to
   * use the condition.
   */
  _Atomic size_t in_flight;
  pthread_mutex_t in_flight_mutex;
  pthread_cond_t in_flight_cond;
} *threads;

static pthread_barrier_t barrier;

void
start_operations_threads (void)
{
  size_t i;
  int err;

  /* This barrier is used to ensure all operations threads have
   * started up before we leave this function.
   */
  err = pthread_barrier_init (&barrier, NULL, nbd.size + 1);
  if (err != 0) {
    errno = err;
    perror ("nbdfuse: pthread_barrier_init");
    exit (EXIT_FAILURE);
  }

  threads = calloc (nbd.size, sizeof (struct thread));
  if (threads == NULL) {
    perror ("calloc");
    exit (EXIT_FAILURE);
  }

  for (i = 0; i < nbd.size; ++i) {
    threads[i].n = i;
    threads[i].in_flight = 0;
    if ((err = pthread_mutex_init (&threads[i].in_flight_mutex, NULL)) != 0 ||
        (err = pthread_cond_init (&threads[i].in_flight_cond, NULL)) != 0) {
      errno = err;
      perror ("nbdfuse: mutex/cond init");
      exit (EXIT_FAILURE);
    }
    err = pthread_create (&threads[i].thread, NULL,
                          operations_thread, &threads[i]);
    if (err != 0) {
      errno = err;
      perror ("nbdfuse: pthread_create");
      exit (EXIT_FAILURE);
    }
  }

  /* Wait on the barrier. */
  pthread_barrier_wait (&barrier);
  pthread_barrier_destroy (&barrier);
}

struct completion {
  pthread_mutex_t mutex;
  pthread_cond_t cond;
  bool completed;
  struct thread *thread;
} completion;

static void *
operations_thread (void *arg)
{
  struct thread *thread = arg;
  size_t n = thread->n;
  struct nbd_handle *h = nbd.ptr[n];

  /* Signal to the main thread that we have initialized. */
  pthread_barrier_wait (&barrier);

  while (1) {
    /* Sleep until at least one command is in flight. */
    pthread_mutex_lock (&thread->in_flight_mutex);
    while (thread->in_flight == 0)
      pthread_cond_wait (&thread->in_flight_cond, &thread->in_flight_mutex);
    pthread_mutex_unlock (&thread->in_flight_mutex);

    /* Dispatch work while there are commands in flight. */
    while (thread->in_flight > 0)
      nbd_poll (h, -1);
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

  /* We have to decrement this here so that the caller (the operations
   * thread) does not reenter nbd_poll.
   */
  assert (completion->thread->in_flight >= 1);
  completion->thread->in_flight--;

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

/* Round-robin assignment of commands to operation threads (and
 * therefore to NBD handle "owned" by that thread).
 */
static size_t
next_thread (void)
{
  static _Atomic size_t n = 0;

  if (nbd.size == 1)
    return 0;
  else {
    size_t i = n++;
    return i % (nbd.size - 1);
  }
}

static int
wait_for_completion (size_t index, struct completion *completion,
                     int64_t cookie)
{
  int r;

  /* Signal to the operations thread to start work, in case it is sleeping. */
  pthread_mutex_lock (&threads[index].in_flight_mutex);
  threads[index].in_flight++;
  pthread_cond_signal (&threads[index].in_flight_cond);
  pthread_mutex_unlock (&threads[index].in_flight_mutex);

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
  r = nbd_aio_command_completed (nbd.ptr[index], cookie);
  assert (r != 0);
  return r;
}

/* Wrap calls to any asynch command and check the error. */
#define CHECK_NBD_ASYNC_ERROR(CALL)                                     \
  do {                                                                  \
    size_t index = next_thread ();                                      \
    struct completion completion =                                      \
      { PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER,            \
        false, &threads[index] };                                       \
    nbd_completion_callback cb =                                        \
      { .callback = completion_callback, .user_data = &completion };    \
    struct nbd_handle *h = nbd.ptr[index];                              \
    int64_t cookie = (CALL);                                            \
    if (cookie == -1 ||                                                 \
        wait_for_completion (index, &completion, cookie) == -1)         \
      return report_nbd_error ();                                       \
  } while (0)

/* Wraps calls to sync libnbd functions and check the error. */
//#define CHECK_NBD_SYNC_ERROR(CALL)
//  do { if ((CALL) == -1) return report_nbd_error (); } while (0)

static int
nbdfuse_getattr (const char *path, struct stat *statbuf,
                 struct fuse_file_info *fi)
{
  const int mode = readonly ? 0444 : 0666;

  DEBUG_OPERATION ("getattr", "path=%s", path);

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
  DEBUG_OPERATION ("readdir", "path=%s", path);

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
  DEBUG_OPERATION ("open", "path=%s, flags=%d", path, fi->flags);

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
  DEBUG_OPERATION ("read", "path=%s, buf=%p, count=%zu, offset=%" PRIi64,
                   path, buf, count, (int64_t) offset);

  if (!file_mode && (path[0] != '/' || strcmp (path+1, filename) != 0))
    return -ENOENT;

  if (offset >= size)
    return 0;

  if (count > MAX_REQUEST_SIZE)
    count = MAX_REQUEST_SIZE;

  if (offset + count > size)
    count = size - offset;

  CHECK_NBD_ASYNC_ERROR (nbd_aio_pread (h, buf, count, offset, cb, 0));

  return (int) count;
}

static int
nbdfuse_write (const char *path, const char *buf,
               size_t count, off_t offset,
               struct fuse_file_info *fi)
{
  DEBUG_OPERATION ("write", "path=%s, buf=%p, count=%zu, offset=%" PRIi64,
                   path, buf, count, (int64_t) offset);

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

  CHECK_NBD_ASYNC_ERROR (nbd_aio_pwrite (h, buf, count, offset, cb, 0));

  return (int) count;
}

static int
nbdfuse_fsync (const char *path, int datasync, struct fuse_file_info *fi)
{
  DEBUG_OPERATION ("fsync", "path=%s, datasync=%d", path, datasync);

  if (readonly)
    return 0;

  /* If the server doesn't support flush then the operation is
   * silently ignored.
   */
  if (nbd_can_flush (nbd.ptr[0]))
    CHECK_NBD_ASYNC_ERROR (nbd_aio_flush (h, cb, 0));

  return 0;
}

#ifndef FALLOC_FL_PUNCH_HOLE
# define FALLOC_FL_PUNCH_HOLE 0
#endif

#ifndef FALLOC_FL_ZERO_RANGE
# define FALLOC_FL_ZERO_RANGE 0
#endif

/* Punch a hole or write zeros. */
static int
nbdfuse_fallocate (const char *path, int mode, off_t offset, off_t len,
                   struct fuse_file_info *fi)
{
  DEBUG_OPERATION ("fallocate", "path=%s, mode=%d, "
                   "offset=%" PRIi64 ", len=%" PRIi64,
                   path, mode, (int64_t) offset, (int64_t) len);

  if (readonly)
    return -EACCES;

  if (mode & FALLOC_FL_PUNCH_HOLE) {
    if (!nbd_can_trim (nbd.ptr[0]))
      return -EOPNOTSUPP;       /* Trim not supported. */
    else {
      CHECK_NBD_ASYNC_ERROR (nbd_aio_trim (h, len, offset, cb, 0));
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
        CHECK_NBD_ASYNC_ERROR (nbd_aio_pwrite (h, zerobuf, n, offset, cb, 0));
        len -= n;
      }
      return 0;
    }
    else {
      CHECK_NBD_ASYNC_ERROR (nbd_aio_zero (h, len, offset, cb, 0));
      return 0;
    }
  }
  else
    return -EOPNOTSUPP;
}

/* This is called when the filesystem is unmounted. */
static void
nbdfuse_destroy (void *data)
{
  time_t st;
  size_t i;

  DEBUG_OPERATION ("destroy", "(no parameters)");

  /* Wait until there are no more commands in flight or until a
   * timeout is reached.
   */
  time (&st);
  while (time (NULL) - st <= RELEASE_TIMEOUT) {
    for (i = 0; i < nbd.size; ++i) {
      if (threads[i].in_flight > 0)
        break;
    }
    if (i == nbd.size) /* no commands in flight */
      break;

    /* Signal to the operations thread to work. */
    for (i = 0; i < nbd.size; ++i) {
      pthread_mutex_lock (&threads[i].in_flight_mutex);
      pthread_cond_signal (&threads[i].in_flight_cond);
      pthread_mutex_unlock (&threads[i].in_flight_mutex);
    }

    sleep (1);
  }
}

struct fuse_operations nbdfuse_operations = {
  .getattr           = nbdfuse_getattr,
  .readdir           = nbdfuse_readdir,
  .open              = nbdfuse_open,
  .read              = nbdfuse_read,
  .write             = nbdfuse_write,
  .fsync             = nbdfuse_fsync,
  .fallocate         = nbdfuse_fallocate,
  .destroy           = nbdfuse_destroy,
};
