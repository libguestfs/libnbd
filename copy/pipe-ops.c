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

#include "nbdcopy.h"

static void
pipe_close (struct rw *rw)
{
  if (close (rw->u.local.fd) == -1) {
    fprintf (stderr, "%s: close: %m\n", rw->name);
    exit (EXIT_FAILURE);
  }
}

static void
pipe_flush (struct rw *rw)
{
  /* We don't need to do anything here as the close will return an
   * error if the pipe could not be flushed.
   */
}

static size_t
pipe_synch_read (struct rw *rw,
                 void *data, size_t len, uint64_t offset)
{
  ssize_t r;

  r = read (rw->u.local.fd, data, len);
  if (r == -1) {
    perror (rw->name);
    exit (EXIT_FAILURE);
  }
  return r;
}

static void
pipe_synch_write (struct rw *rw,
                  const void *data, size_t len, uint64_t offset)
{
  ssize_t r;

  while (len > 0) {
    r = write (rw->u.local.fd, data, len);
    if (r == -1) {
      perror (rw->name);
      exit (EXIT_FAILURE);
    }
    data += r;
    len -= r;
  }
}

static bool
pipe_synch_trim_zero (struct rw *rw, uint64_t offset, uint64_t count)
{
  return false; /* not supported by pipes */
}

static void
pipe_asynch_read (struct rw *rw,
                  struct command *command,
                  nbd_completion_callback cb)
{
  abort (); /* See comment below. */
}

static void
pipe_asynch_write (struct rw *rw,
                   struct command *command,
                   nbd_completion_callback cb)
{
  abort (); /* See comment below. */
}

static bool
pipe_asynch_trim_zero (struct rw *rw, struct command *command,
                       nbd_completion_callback cb)
{
  return false; /* not supported by pipes */
}

static unsigned
pipe_in_flight (struct rw *rw, uintptr_t index)
{
  return 0;
}

struct rw_ops pipe_ops = {
  .ops_name = "pipe_ops",

  .close = pipe_close,
  .flush = pipe_flush,

  .synch_read = pipe_synch_read,
  .synch_write = pipe_synch_write,
  .synch_trim = pipe_synch_trim_zero,
  .synch_zero = pipe_synch_trim_zero,

  /* Asynch pipe read/write operations are not defined.  These should
   * never be called because pipes/streams/sockets force synchronous
   * mode.  Because calling a NULL pointer screws up the stack trace
   * when we're not using frame pointers, these are defined to
   * functions that call abort().
   */
  .asynch_read = pipe_asynch_read,
  .asynch_write = pipe_asynch_write,

  .asynch_trim = pipe_asynch_trim_zero,
  .asynch_zero = pipe_asynch_trim_zero,
  .in_flight = pipe_in_flight,

  .get_polling_fd = get_polling_fd_not_supported,
  .asynch_notify_read = asynch_notify_read_write_not_supported,
  .asynch_notify_write = asynch_notify_read_write_not_supported,

  .get_extents = default_get_extents,
};
