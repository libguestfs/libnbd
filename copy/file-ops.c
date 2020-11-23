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

struct rw_ops file_ops = {
  .synch_read = file_synch_read,
  .synch_write = file_synch_write,
  .asynch_read = file_asynch_read,
  .asynch_write = file_asynch_write,
};
