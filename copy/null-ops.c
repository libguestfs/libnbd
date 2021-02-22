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
 * should be faster than using /dev/null because it "supports" fast
 * zeroing.
 */

static struct rw_ops null_ops;

struct rw_null {
  struct rw rw;
};

struct rw *
null_create (const char *name)
{
  struct rw_null *rw = calloc (1, sizeof *rw);
  if (rw == NULL) { perror ("calloc"); exit (EXIT_FAILURE); }

  rw->rw.ops = &null_ops;
  rw->rw.name = name;
  rw->rw.size = INT64_MAX;
  return &rw->rw;
}

static void
null_close (struct rw *rw)
{
  free (rw);
}

static void
null_flush (struct rw *rw)
{
  /* nothing */
}

static bool
null_is_read_only (struct rw *rw)
{
  return false;
}

static bool
null_can_extents (struct rw *rw)
{
  return false;
}

static bool
null_can_multi_conn (struct rw *rw)
{
  return true;
}

static void
null_start_multi_conn (struct rw *rw)
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
null_synch_zero (struct rw *rw, uint64_t offset, uint64_t count, bool allocate)
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
null_asynch_zero (struct rw *rw, struct command *command,
                  nbd_completion_callback cb, bool allocate)
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

static struct rw_ops null_ops = {
  .ops_name = "null_ops",
  .close = null_close,
  .is_read_only = null_is_read_only,
  .can_extents = null_can_extents,
  .can_multi_conn = null_can_multi_conn,
  .start_multi_conn = null_start_multi_conn,
  .flush = null_flush,
  .synch_read = null_synch_read,
  .synch_write = null_synch_write,
  .synch_zero = null_synch_zero,
  .asynch_read = null_asynch_read,
  .asynch_write = null_asynch_write,
  .asynch_zero = null_asynch_zero,
  .in_flight = null_in_flight,
  .get_polling_fd = get_polling_fd_not_supported,
  .asynch_notify_read = asynch_notify_read_write_not_supported,
  .asynch_notify_write = asynch_notify_read_write_not_supported,
  .get_extents = null_get_extents,
};
