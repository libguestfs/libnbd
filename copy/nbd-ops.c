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

#include <libnbd.h>

#include "nbdcopy.h"

static size_t
nbd_synch_read (struct rw *rw,
                void *data, size_t len, uint64_t offset)
{
  assert (rw->t == NBD);

  if (len > rw->size - offset)
    len = rw->size - offset;
  if (len == 0)
    return 0;

  if (nbd_pread (rw->u.nbd.ptr[0], data, len, offset, 0) == -1) {
    fprintf (stderr, "%s: %s\n", rw->name, nbd_get_error ());
    exit (EXIT_FAILURE);
  }

  return len;
}

static void
nbd_synch_write (struct rw *rw,
                 const void *data, size_t len, uint64_t offset)
{
  assert (rw->t == NBD);

  if (nbd_pwrite (rw->u.nbd.ptr[0], data, len, offset, 0) == -1) {
    fprintf (stderr, "%s: %s\n", rw->name, nbd_get_error ());
    exit (EXIT_FAILURE);
  }
}

static void
nbd_asynch_read (struct rw *rw,
                 struct buffer *buffer,
                 nbd_completion_callback cb)
{
  assert (rw->t == NBD);

  if (nbd_aio_pread (rw->u.nbd.ptr[buffer->index],
                     buffer->data, buffer->len, buffer->offset,
                     cb, 0) == -1) {
    fprintf (stderr, "%s: %s\n", rw->name, nbd_get_error ());
    exit (EXIT_FAILURE);
  }
}

static void
nbd_asynch_write (struct rw *rw,
                  struct buffer *buffer,
                  nbd_completion_callback cb)
{
  assert (rw->t == NBD);

  if (nbd_aio_pwrite (rw->u.nbd.ptr[buffer->index],
                      buffer->data, buffer->len, buffer->offset,
                      cb, 0) == -1) {
    fprintf (stderr, "%s: %s\n", rw->name, nbd_get_error ());
    exit (EXIT_FAILURE);
  }
}

struct rw_ops nbd_ops = {
  .synch_read = nbd_synch_read,
  .synch_write = nbd_synch_write,
  .asynch_read = nbd_asynch_read,
  .asynch_write = nbd_asynch_write,
};
