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
#include <stdint.h>
#include <stdbool.h>
#include <fcntl.h>
#include <unistd.h>
#include <poll.h>
#include <errno.h>
#include <assert.h>

#include <pthread.h>

#include <libnbd.h>

#include "nbdcopy.h"

/* Threads pick up work in units of THREAD_WORK_SIZE starting at the
 * next_offset.  The lock protects next_offset.
 */
static uint64_t next_offset = 0;
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

static bool
get_next_offset (uint64_t *offset, uint64_t *count)
{
  bool r = false;               /* returning false means no more work */

  pthread_mutex_lock (&lock);
  if (next_offset < src.size) {
    *offset = next_offset;

    /* Work out how large this range is.  The last range may be
     * smaller than THREAD_WORK_SIZE.
     */
    *count = src.size - *offset;
    if (*count > THREAD_WORK_SIZE)
      *count = THREAD_WORK_SIZE;

    next_offset += THREAD_WORK_SIZE;
    r = true;                   /* there is more work */

    /* XXX This means the progress bar "runs fast" since it shows the
     * progress issuing commands, not necessarily progress performing
     * the commands.  We might move this into a callback, but those
     * are called from threads and not necessarily in monotonic order
     * so the progress bar would move erratically.
     */
    if (progress)
      progress_bar (*offset, dst.size);
  }
  pthread_mutex_unlock (&lock);
  return r;
}

static void *worker_thread (void *ip);

void
multi_thread_copying (void)
{
  pthread_t *workers;
  size_t i;
  int err;

  /* Some invariants that should be true if the main program called us
   * correctly.
   */
  assert (threads > 0);
  assert (threads == connections);
  if (src.t == NBD)
    assert (src.u.nbd.size == connections);
  if (dst.t == NBD)
    assert (dst.u.nbd.size == connections);
  assert (src.size != -1);

  workers = malloc (sizeof (pthread_t) * threads);
  if (workers == NULL) {
    perror ("malloc");
    exit (EXIT_FAILURE);
  }

  /* Start the worker threads. */
  for (i = 0; i < threads; ++i) {
    err = pthread_create (&workers[i], NULL, worker_thread,
                          (void *)(uintptr_t)i);
    if (err != 0) {
      errno = err;
      perror ("pthread_create");
      exit (EXIT_FAILURE);
    }
  }

  /* Wait until all worker threads exit. */
  for (i = 0; i < threads; ++i) {
    err = pthread_join (workers[i], NULL);
    if (err != 0) {
      errno = err;
      perror ("pthread_join");
      exit (EXIT_FAILURE);
    }
  }

  free (workers);
}

static unsigned in_flight (struct nbd_handle *src_nbd,
                           struct nbd_handle *dst_nbd);
static void poll_both_ends (struct nbd_handle *src_nbd,
                            struct nbd_handle *dst_nbd);
static int finished_read (void *vp, int *error);
static int finished_write (void *vp, int *error);

/* There are 'threads' worker threads, each copying work ranges from
 * src to dst until there are no more work ranges.
 */
static void *
worker_thread (void *indexp)
{
  uintptr_t index = (uintptr_t) indexp;
  uint64_t offset, count;
  struct nbd_handle *src_nbd, *dst_nbd;
  bool done = false;

  if (! get_next_offset (&offset, &count))
    /* No work to do, return immediately.  Can happen for files which
     * are smaller than THREAD_WORK_SIZE where multi-conn is enabled.
     */
    return NULL;

  /* In the case where src or dst is NBD, use
   * {src|dst}.u.nbd.ptr[index] so that each thread is connected to
   * its own NBD connection.  If either src or dst is LOCAL then set
   * src_nbd/dst_nbd to NULL so hopefully we'll crash hard if the
   * program accidentally tries to use them.
   */
  if (src.t == NBD)
    src_nbd = src.u.nbd.ptr[index];
  else
    src_nbd = NULL;
  if (dst.t == NBD)
    dst_nbd = dst.u.nbd.ptr[index];
  else
    dst_nbd = NULL;

  while (!done) {
    struct buffer *buffer;
    char *data;
    size_t len;

    if (count == 0) {
      /* Get another work range. */
      done = ! get_next_offset (&offset, &count);
      if (done) break;
      assert (0 < count && count <= THREAD_WORK_SIZE);
    }

    /* If the number of requests in flight exceeds the limit, poll
     * waiting for at least one request to finish.  This enforces the
     * user --requests option.
     */
    while (in_flight (src_nbd, dst_nbd) >= max_requests)
      poll_both_ends (src_nbd, dst_nbd);

    /* Create a new buffer.  This will be freed in a callback handler. */
    len = count;
    if (len > MAX_REQUEST_SIZE)
      len = MAX_REQUEST_SIZE;
    data = malloc (len);
    if (data == NULL) {
      perror ("malloc");
      exit (EXIT_FAILURE);
    }
    buffer = malloc (sizeof *buffer);
    if (buffer == NULL) {
      perror ("malloc");
      exit (EXIT_FAILURE);
    }
    buffer->offset = offset;
    buffer->len = len;
    buffer->data = data;
    buffer->free_data = free;
    buffer->index = index;

    /* Begin the asynch read operation. */
    src.ops->asynch_read (&src, buffer,
                          (nbd_completion_callback) {
                            .callback = finished_read,
                            .user_data = buffer,
                          });

    offset += len;
    count -= len;
  }

  /* Wait for in flight NBD requests to finish. */
  while (in_flight (src_nbd, dst_nbd) > 0)
    poll_both_ends (src_nbd, dst_nbd);

  if (progress)
    progress_bar (1, 1);

  return NULL;
}

/* Count the number of NBD commands in flight.  Since the commands are
 * auto-retired in the callbacks we don't need to count "done"
 * commands.
 */
static inline unsigned
in_flight (struct nbd_handle *src_nbd, struct nbd_handle *dst_nbd)
{
  return
    (src_nbd ? nbd_aio_in_flight (src_nbd) : 0) +
    (dst_nbd ? nbd_aio_in_flight (dst_nbd) : 0);
}

/* Poll (optional) NBD src and NBD dst, moving the state machine(s)
 * along.  This is a lightly modified nbd_poll.
 */
static void
poll_both_ends (struct nbd_handle *src_nbd, struct nbd_handle *dst_nbd)
{
  struct pollfd fds[2] = { 0 };
  int r;

  /* Note: poll will ignore fd == -1 */

  if (!src_nbd)
    fds[0].fd = -1;
  else {
    fds[0].fd = nbd_aio_get_fd (src_nbd);
    switch (nbd_aio_get_direction (src_nbd)) {
    case LIBNBD_AIO_DIRECTION_READ:
      fds[0].events = POLLIN;
      break;
    case LIBNBD_AIO_DIRECTION_WRITE:
      fds[0].events = POLLOUT;
      break;
    case LIBNBD_AIO_DIRECTION_BOTH:
      fds[0].events = POLLIN|POLLOUT;
      break;
    default:
      fprintf (stderr, "%s: %s\n", src.name, nbd_get_error ());
      exit (EXIT_FAILURE);
    }
  }

  if (!dst_nbd)
    fds[1].fd = -1;
  else {
    fds[1].fd = nbd_aio_get_fd (dst_nbd);
    switch (nbd_aio_get_direction (dst_nbd)) {
    case LIBNBD_AIO_DIRECTION_READ:
      fds[1].events = POLLIN;
      break;
    case LIBNBD_AIO_DIRECTION_WRITE:
      fds[1].events = POLLOUT;
      break;
    case LIBNBD_AIO_DIRECTION_BOTH:
      fds[1].events = POLLIN|POLLOUT;
      break;
    default:
      fprintf (stderr, "%s: %s\n", src.name, nbd_get_error ());
      exit (EXIT_FAILURE);
    }
  }

  r = poll (fds, 2, -1);
  if (r == -1) {
    perror ("poll");
    exit (EXIT_FAILURE);
  }
  if (r == 0)
    return;

  if (src_nbd) {
    r = 0;
    if ((fds[0].revents & (POLLIN | POLLHUP)) != 0)
      r = nbd_aio_notify_read (src_nbd);
    else if ((fds[0].revents & POLLOUT) != 0)
      r = nbd_aio_notify_write (src_nbd);
    else if ((fds[0].revents & (POLLERR | POLLNVAL)) != 0) {
      errno = ENOTCONN;
      r = -1;
    }
    if (r == -1) {
      perror (src.name);
      exit (EXIT_FAILURE);
    }
  }

  if (dst_nbd) {
    r = 0;
    if ((fds[1].revents & (POLLIN | POLLHUP)) != 0)
      r = nbd_aio_notify_read (dst_nbd);
    else if ((fds[1].revents & POLLOUT) != 0)
      r = nbd_aio_notify_write (dst_nbd);
    else if ((fds[1].revents & (POLLERR | POLLNVAL)) != 0) {
      errno = ENOTCONN;
      r = -1;
    }
    if (r == -1) {
      perror (dst.name);
      exit (EXIT_FAILURE);
    }
  }
}

/* Callback called when src has finished one read command.  This
 * initiates a write.
 */
static int
finished_read (void *vp, int *error)
{
  struct buffer *buffer = vp;

  dst.ops->asynch_write (&dst, buffer,
                         (nbd_completion_callback) {
                           .callback = finished_write,
                           .user_data = buffer,
                         });

  return 1; /* auto-retires the command */
}

/* Callback called when dst has finished one write command.  We can
 * now free the buffer.
 */
static int
finished_write (void *vp, int *error)
{
  struct buffer *buffer = vp;

  if (buffer->free_data) buffer->free_data (buffer->data);
  free (buffer);
  return 1; /* auto-retires the command */
}
