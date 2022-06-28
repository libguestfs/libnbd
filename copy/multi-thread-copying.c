/* NBD client library in userspace.
 * Copyright (C) 2020-2022 Red Hat Inc.
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
#include <sys/stat.h>
#include <inttypes.h>

#include <pthread.h>

#include <libnbd.h>

#include "iszero.h"
#include "minmax.h"
#include "rounding.h"

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
  if (next_offset < src->size) {
    *offset = next_offset;

    /* Work out how large this range is.  The last range may be
     * smaller than THREAD_WORK_SIZE.
     */
    *count = src->size - *offset;
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
    progress_bar (*offset, src->size);
  }
  pthread_mutex_unlock (&lock);
  return r;
}

static void *worker_thread (void *wp);

void
multi_thread_copying (void)
{
  struct worker *workers;
  size_t i;
  int err;

  /* Some invariants that should be true if the main program called us
   * correctly.
   */
  assert (threads > 0);
  assert (threads == connections);
/*
  if (src.ops == &nbd_ops)
    assert (src.u.nbd.handles.size == connections);
  if (dst.ops == &nbd_ops)
    assert (dst.u.nbd.handles.size == connections);
*/
  assert (src->size != -1);

  workers = calloc (threads, sizeof *workers);
  if (workers == NULL) {
    perror ("calloc");
    exit (EXIT_FAILURE);
  }

  /* Start the worker threads. */
  for (i = 0; i < threads; ++i) {
    workers[i].index = i;
    err = pthread_create (&workers[i].thread, NULL, worker_thread,
                          &workers[i]);
    if (err != 0) {
      errno = err;
      perror ("pthread_create");
      exit (EXIT_FAILURE);
    }
  }

  /* Wait until all worker threads exit. */
  for (i = 0; i < threads; ++i) {
    err = pthread_join (workers[i].thread, NULL);
    if (err != 0) {
      errno = err;
      perror ("pthread_join");
      exit (EXIT_FAILURE);
    }
  }

  free (workers);
}

static void wait_for_request_slots (struct worker *worker);
static unsigned in_flight (size_t index);
static void poll_both_ends (size_t index);
static int finished_read (void *vp, int *error);
static int finished_command (void *vp, int *error);
static void free_command (struct command *command);
static void fill_dst_range_with_zeroes (struct command *command);
static struct command *create_command (uint64_t offset, size_t len, bool zero,
                                       struct worker *worker);

/* Tracking worker queue size.
 *
 * The queue size is increased when starting a read command.
 *
 * The queue size is decreased when a read command is converted to zero
 * subcommand in finished_read(), or when a write command completes in
 * finished_command().
 *
 * Zero commands are not considered in the queue size since they have no
 * payload.
 */

static inline void
increase_queue_size (struct worker *worker, size_t len)
{
  assert (worker->queue_size < queue_size);
  worker->queue_size += len;
}

static inline void
decrease_queue_size (struct worker *worker, size_t len)
{
  assert (worker->queue_size >= len);
  worker->queue_size -= len;
}

/* Using the extents map 'exts', check if the region
 * [offset..offset+len-1] intersects only with zero extents.
 *
 * The invariant for '*i' is always an extent which starts before or
 * equal to the current offset.
 */
static bool
only_zeroes (const extent_list exts, size_t *i,
             uint64_t offset, unsigned len)
{
  size_t j;

  /* Invariant. */
  assert (*i < exts.len);
  assert (exts.ptr[*i].offset <= offset);

  /* Update the invariant.  Search for the last possible extent in the
   * list which is <= offset.
   */
  for (j = *i + 1; j < exts.len; ++j) {
    if (exts.ptr[j].offset <= offset)
      *i = j;
    else
      break;
  }

  /* Check invariant again. */
  assert (*i < exts.len);
  assert (exts.ptr[*i].offset <= offset);

  /* If *i is not the last extent, then the next extent starts
   * strictly beyond our current offset.
   */
  assert (*i == exts.len - 1 || exts.ptr[*i + 1].offset > offset);

  /* Search forward, look for any non-zero extents overlapping the region. */
  for (j = *i; j < exts.len; ++j) {
    uint64_t start, end;

    /* [start..end-1] is the current extent. */
    start = exts.ptr[j].offset;
    end = exts.ptr[j].offset + exts.ptr[j].length;

    assert (end > offset);

    if (start >= offset + len)
      break;

    /* Non-zero extent covering this region => test failed. */
    if (!exts.ptr[j].zero)
      return false;
  }

  return true;
}

/* There are 'threads' worker threads, each copying work ranges from
 * src to dst until there are no more work ranges.
 */
static void *
worker_thread (void *wp)
{
  struct worker *w = wp;
  uint64_t offset, count;
  extent_list exts = empty_vector;

  while (get_next_offset (&offset, &count)) {
    struct command *command;
    size_t extent_index;
    bool is_zeroing = false;
    uint64_t zeroing_start = 0; /* initialized to avoid bogus GCC warning */

    assert (0 < count && count <= THREAD_WORK_SIZE);
    if (extents)
      src->ops->get_extents (src, w->index, offset, count, &exts);
    else
      default_get_extents (src, w->index, offset, count, &exts);

    extent_index = 0; // index into extents array used to optimize only_zeroes
    while (count) {
      const size_t len = MIN (count, request_size);

      if (only_zeroes (exts, &extent_index, offset, len)) {
        /* The source is zero so we can proceed directly to skipping,
         * fast zeroing, or writing zeroes at the destination.  Defer
         * zeroing so we can send it as a single large command.
         */
        if (!is_zeroing) {
          is_zeroing = true;
          zeroing_start = offset;
        }
      }
      else /* data */ {
        /* If we were in the middle of deferred zeroing, do it now. */
        if (is_zeroing) {
          /* Note that offset-zeroing_start can never exceed
           * THREAD_WORK_SIZE, so there is no danger of overflowing
           * size_t.
           */
          command = create_command (zeroing_start, offset-zeroing_start,
                                    true, w);
          fill_dst_range_with_zeroes (command);
          is_zeroing = false;
        }

        /* Issue the asynchronous read command. */
        command = create_command (offset, len, false, w);

        wait_for_request_slots (w);

        /* NOTE: Must increase the queue size after waiting. */
        increase_queue_size (w, len);

        /* Begin the asynch read operation. */
        src->ops->asynch_read (src, command,
                               (nbd_completion_callback) {
                                 .callback = finished_read,
                                 .user_data = command,
                               });
      }

      offset += len;
      count -= len;
    } /* while (count) */

    /* If we were in the middle of deferred zeroing, do it now. */
    if (is_zeroing) {
      /* Note that offset-zeroing_start can never exceed
       * THREAD_WORK_SIZE, so there is no danger of overflowing
       * size_t.
       */
      command = create_command (zeroing_start, offset - zeroing_start,
                                true, w);
      fill_dst_range_with_zeroes (command);
      is_zeroing = false;
    }
  }

  /* Wait for in flight NBD requests to finish. */
  while (in_flight (w->index) > 0)
    poll_both_ends (w->index);

  free (exts.ptr);
  return NULL;
}

/* If the number of requests or queued bytes in flight exceed limits,
 * then poll until enough requests finish.  This enforces the user
 * --requests and --queue-size options.
 *
 * NB: Unfortunately it's not possible to call this from a callback,
 * since it will deadlock trying to grab the libnbd handle lock.  This
 * means that although the worker thread calls this and enforces the
 * limit, when we split up requests into subrequests (eg. doing
 * sparseness detection) we will probably exceed the user request
 * limit. XXX
 */
static void
wait_for_request_slots (struct worker *worker)
{
  while (in_flight (worker->index) >= max_requests ||
         worker->queue_size >= queue_size)
    poll_both_ends (worker->index);
}

/* Count the number of asynchronous commands in flight. */
static unsigned
in_flight (size_t index)
{
  return src->ops->in_flight (src, index) + dst->ops->in_flight (dst, index);
}

/* Poll (optional) NBD src and NBD dst, moving the state machine(s)
 * along.  This is a lightly modified nbd_poll.
 */
static void
poll_both_ends (size_t index)
{
  struct pollfd fds[2];
  int r, direction;

  memset (fds, 0, sizeof fds);

  /* Note: if polling is not supported, this function will
   * set fd == -1 which poll ignores.
   */
  src->ops->get_polling_fd (src, index, &fds[0].fd, &direction);
  if (fds[0].fd >= 0) {
    switch (direction) {
    case LIBNBD_AIO_DIRECTION_READ:
      fds[0].events = POLLIN;
      break;
    case LIBNBD_AIO_DIRECTION_WRITE:
      fds[0].events = POLLOUT;
      break;
    case LIBNBD_AIO_DIRECTION_BOTH:
      fds[0].events = POLLIN|POLLOUT;
      break;
    }
  }

  dst->ops->get_polling_fd (dst, index, &fds[1].fd, &direction);
  if (fds[1].fd >= 0) {
    switch (direction) {
    case LIBNBD_AIO_DIRECTION_READ:
      fds[1].events = POLLIN;
      break;
    case LIBNBD_AIO_DIRECTION_WRITE:
      fds[1].events = POLLOUT;
      break;
    case LIBNBD_AIO_DIRECTION_BOTH:
      fds[1].events = POLLIN|POLLOUT;
      break;
    }
  }

  r = poll (fds, 2, -1);
  if (r == -1) {
    perror ("poll");
    exit (EXIT_FAILURE);
  }
  if (r == 0)
    return;

  if (fds[0].fd >= 0) {
    if ((fds[0].revents & (POLLIN | POLLHUP)) != 0)
      src->ops->asynch_notify_read (src, index);
    else if ((fds[0].revents & POLLOUT) != 0)
      src->ops->asynch_notify_write (src, index);
    else if ((fds[0].revents & (POLLERR | POLLNVAL)) != 0) {
      errno = ENOTCONN;
      perror (src->name);
      exit (EXIT_FAILURE);
    }
  }

  if (fds[1].fd >= 0) {
    if ((fds[1].revents & (POLLIN | POLLHUP)) != 0)
      dst->ops->asynch_notify_read (dst, index);
    else if ((fds[1].revents & POLLOUT) != 0)
      dst->ops->asynch_notify_write (dst, index);
    else if ((fds[1].revents & (POLLERR | POLLNVAL)) != 0) {
      errno = ENOTCONN;
      perror (dst->name);
      exit (EXIT_FAILURE);
    }
  }
}

/* Create a new buffer. */
static struct buffer*
create_buffer (size_t len)
{
  struct buffer *buffer;

  buffer = calloc (1, sizeof *buffer);
  if (buffer == NULL) {
    perror ("calloc");
    exit (EXIT_FAILURE);
  }

  buffer->data = malloc (len);
  if (buffer->data == NULL) {
    perror ("malloc");
    exit (EXIT_FAILURE);
  }

  buffer->refs = 1;

  return buffer;
}

/* Create a new command for read or zero. */
static struct command *
create_command (uint64_t offset, size_t len, bool zero, struct worker *worker)
{
  struct command *command;

  command = calloc (1, sizeof *command);
  if (command == NULL) {
    perror ("calloc");
    exit (EXIT_FAILURE);
  }

  command->offset = offset;
  command->slice.len = len;

  if (!zero)
    command->slice.buffer = create_buffer (len);

  command->worker = worker;

  return command;
}

/* Create a sub-command of an existing command.  This creates a slice
 * referencing the buffer of the existing command without copying.
 */
static struct command *
create_subcommand (struct command *command, uint64_t offset, size_t len,
                   bool zero)
{
  const uint64_t end = command->offset + command->slice.len;
  struct command *newcommand;

  assert (command->offset <= offset && offset < end);
  assert (offset + len <= end);

  newcommand = calloc (1, sizeof *newcommand);
  if (newcommand == NULL) {
    perror ("calloc");
    exit (EXIT_FAILURE);
  }
  newcommand->offset = offset;
  newcommand->slice.len = len;
  if (!zero) {
    newcommand->slice.buffer = command->slice.buffer;
    newcommand->slice.buffer->refs++;
    newcommand->slice.base = offset - command->offset;
  }
  newcommand->worker = command->worker;

  return newcommand;
}

/* Callback called when src has finished one read command.  This
 * initiates a write.
 */
static int
finished_read (void *vp, int *error)
{
  struct command *command = vp;

  if (*error) {
    fprintf (stderr, "%s: read at offset %" PRId64 " failed: %s\n",
             prog, command->offset, strerror (*error));
    exit (EXIT_FAILURE);
  }

  if (allocated || sparse_size == 0) {
    /* If sparseness detection (see below) is turned off then we write
     * the whole command.
     */
    dst->ops->asynch_write (dst, command,
                            (nbd_completion_callback) {
                              .callback = finished_command,
                              .user_data = command,
                            });
  }
  else {                               /* Sparseness detection. */
    const uint64_t start = command->offset;
    const uint64_t end = start + command->slice.len;
    uint64_t last_offset = start;
    bool last_is_zero = false;
    uint64_t i;
    struct command *newcommand;

    /* Iterate over whole blocks in the command, starting on a block
     * boundary.
     */
    for (i = MIN (ROUND_UP (start, sparse_size), end);
         i + sparse_size <= end;
         i += sparse_size) {
      if (is_zero (slice_ptr (command->slice) + i-start, sparse_size)) {
        /* It's a zero range.  If the last was a zero too then we do
         * nothing here which coalesces.  Otherwise write the last data
         * and start a new zero range.
         */
        if (!last_is_zero) {
          /* Write the last data (if any). */
          if (i - last_offset > 0) {
            newcommand = create_subcommand (command,
                                            last_offset, i - last_offset,
                                            false);
            dst->ops->asynch_write (dst, newcommand,
                                    (nbd_completion_callback) {
                                      .callback = finished_command,
                                      .user_data = newcommand,
                                    });
          }
          /* Start the new zero range. */
          last_offset = i;
          last_is_zero = true;
        }
      }
      else {
        /* It's data.  If the last was data too, do nothing =>
         * coalesce.  Otherwise write the last zero range and start a
         * new data.
         */
        if (last_is_zero) {
          /* Write the last zero range (if any). */
          if (i - last_offset > 0) {
            newcommand = create_subcommand (command,
                                            last_offset, i - last_offset,
                                            true);
            decrease_queue_size (command->worker, newcommand->slice.len);
            fill_dst_range_with_zeroes (newcommand);
          }
          /* Start the new data. */
          last_offset = i;
          last_is_zero = false;
        }
      }
    } /* for i */

    /* Write the last_offset up to i. */
    if (i - last_offset > 0) {
      if (!last_is_zero) {
        newcommand = create_subcommand (command,
                                        last_offset, i - last_offset,
                                        false);
        dst->ops->asynch_write (dst, newcommand,
                                (nbd_completion_callback) {
                                  .callback = finished_command,
                                  .user_data = newcommand,
                                });
      }
      else {
        newcommand = create_subcommand (command,
                                        last_offset, i - last_offset,
                                        true);
        decrease_queue_size (command->worker, newcommand->slice.len);
        fill_dst_range_with_zeroes (newcommand);
      }
    }

    /* There may be an unaligned tail, so write that. */
    if (end - i > 0) {
      newcommand = create_subcommand (command, i, end - i, false);
      dst->ops->asynch_write (dst, newcommand,
                              (nbd_completion_callback) {
                                .callback = finished_command,
                                .user_data = newcommand,
                              });
    }

    /* Free the original command since it has been split into
     * subcommands and the original is no longer needed.
     */
    free_command (command);
  }

  return 1; /* auto-retires the command */
}

/* Fill a range in dst with zeroes.  This is called from the copying
 * loop when we see a zero range in the source.  Depending on the
 * command line flags this could mean:
 *
 * --destination-is-zero:
 *                 do nothing
 *
 * --allocated:    write zeroes allocating space using an efficient
 *                 zeroing command or writing a command of zeroes
 *
 * (neither flag)  write zeroes punching a hole using an efficient
 *                 zeroing command or fallback to writing a command
 *                 of zeroes.
 *
 * This takes over ownership of the command and frees it eventually.
 */
static void
fill_dst_range_with_zeroes (struct command *command)
{
  char *data;
  size_t data_size;

  if (destination_is_zero)
    goto free_and_return;

  /* Try efficient zeroing. */
  if (dst->ops->asynch_zero (dst, command,
                             (nbd_completion_callback) {
                               .callback = finished_command,
                               .user_data = command,
                             },
                             allocated))
    return;

  /* Fall back to loop writing zeroes.  This is going to be slow
   * anyway, so do it synchronously. XXX
   */
  data_size = MIN (request_size, command->slice.len);
  data = calloc (1, data_size);
  if (!data) {
    perror ("calloc");
    exit (EXIT_FAILURE);
  }
  while (command->slice.len > 0) {
    size_t len = command->slice.len;

    if (len > data_size)
      len = data_size;

    dst->ops->synch_write (dst, data, len, command->offset);
    command->slice.len -= len;
    command->offset += len;
  }
  free (data);

 free_and_return:
  free_command (command);
}

static int
finished_command (void *vp, int *error)
{
  struct command *command = vp;

  if (*error) {
    fprintf (stderr, "%s: write at offset %" PRId64 " failed: %s\n",
             prog, command->offset, strerror (*error));
    exit (EXIT_FAILURE);
  }

  if (command->slice.buffer)
    decrease_queue_size (command->worker, command->slice.len);

  free_command (command);

  return 1; /* auto-retires the command */
}

static void
free_command (struct command *command)
{
  if (command == NULL)
    return;

  struct buffer *buffer = command->slice.buffer;

  if (buffer != NULL) {
    if (--buffer->refs == 0) {
      free (buffer->data);
      free (buffer);
    }
  }

  free (command);
}
