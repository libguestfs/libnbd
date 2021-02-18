/* NBD client library in userspace.
 * Copyright (C) 2020-2021 Red Hat Inc.
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
#include <errno.h>

#include "nbdcopy.h"

/* This sinks writes and aborts on any read-like operations.  It
 * should be faster than using /dev/null because it "supports" trim
 * and fast zeroing.
 */

static void
null_close (struct rw *rw)
{
  /* nothing */
}

static void
null_flush (struct rw *rw)
{
  /* nothing */
}

static size_t
null_synch_read (struct rw *rw,
                 void *data, size_t len, uint64_t offset)
{
  abort ();
}

static void
null_synch_write (struct rw *rw,
                  const void *data, size_t len, uint64_t offset)
{
  /* nothing */
}

static bool
null_synch_trim (struct rw *rw, uint64_t offset, uint64_t count)
{
  return true;
}

static bool
null_synch_zero (struct rw *rw, uint64_t offset, uint64_t count)
{
  return true;
}

static void
null_asynch_read (struct rw *rw,
                  struct command *command,
                  nbd_completion_callback cb)
{
  abort ();
}

static void
null_asynch_write (struct rw *rw,
                   struct command *command,
                   nbd_completion_callback cb)
{
  errno = 0;
  if (cb.callback (cb.user_data, &errno) == -1) {
    perror (rw->name);
    exit (EXIT_FAILURE);
  }
}

static bool
null_asynch_trim (struct rw *rw, struct command *command,
                  nbd_completion_callback cb)
{
  errno = 0;
  if (cb.callback (cb.user_data, &errno) == -1) {
    perror (rw->name);
    exit (EXIT_FAILURE);
  }
  return true;
}

static bool
null_asynch_zero (struct rw *rw, struct command *command,
                  nbd_completion_callback cb)
{
  errno = 0;
  if (cb.callback (cb.user_data, &errno) == -1) {
    perror (rw->name);
    exit (EXIT_FAILURE);
  }
  return true;
}

static unsigned
null_in_flight (struct rw *rw, uintptr_t index)
{
  return 0;
}

static void
null_get_extents (struct rw *rw, uintptr_t index,
                  uint64_t offset, uint64_t count,
                  extent_list *ret)
{
  abort ();
}

struct rw_ops null_ops = {
  .ops_name = "null_ops",
  .close = null_close,
  .flush = null_flush,
  .synch_read = null_synch_read,
  .synch_write = null_synch_write,
  .synch_trim = null_synch_trim,
  .synch_zero = null_synch_zero,
  .asynch_read = null_asynch_read,
  .asynch_write = null_asynch_write,
  .asynch_trim = null_asynch_trim,
  .asynch_zero = null_asynch_zero,
  .in_flight = null_in_flight,
  .get_polling_fd = get_polling_fd_not_supported,
  .asynch_notify_read = asynch_notify_read_write_not_supported,
  .asynch_notify_write = asynch_notify_read_write_not_supported,
  .get_extents = null_get_extents,
};
