/* NBD client library in userspace
 * Copyright (C) 2013-2019 Red Hat Inc.
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

#include "internal.h"

static int
get_flag (struct nbd_handle *h, uint16_t flag)
{
  if (h->eflags == 0) {
    set_error (EINVAL, "server has not returned export flags, "
               "you need to connect to the server first");
    return -1;
  }

  return (h->eflags & flag) != 0;
}

int
nbd_unlocked_read_only (struct nbd_handle *h)
{
  return get_flag (h, NBD_FLAG_READ_ONLY);
}

int
nbd_unlocked_can_flush (struct nbd_handle *h)
{
  return get_flag (h, NBD_FLAG_SEND_FLUSH);
}

int
nbd_unlocked_can_fua (struct nbd_handle *h)
{
  return get_flag (h, NBD_FLAG_SEND_FUA);
}

int
nbd_unlocked_is_rotational (struct nbd_handle *h)
{
  return get_flag (h, NBD_FLAG_ROTATIONAL);
}

int
nbd_unlocked_can_trim (struct nbd_handle *h)
{
  return get_flag (h, NBD_FLAG_SEND_TRIM);
}

int
nbd_unlocked_can_zero (struct nbd_handle *h)
{
  return get_flag (h, NBD_FLAG_SEND_WRITE_ZEROES);
}

int
nbd_unlocked_can_multi_conn (struct nbd_handle *h)
{
  return get_flag (h, NBD_FLAG_CAN_MULTI_CONN);
}

int
nbd_unlocked_can_cache (struct nbd_handle *h)
{
  return get_flag (h, NBD_FLAG_SEND_CACHE);
}

int
nbd_unlocked_can_meta_context (struct nbd_handle *h, const char *name)
{
  struct nbd_connection *conn = NULL;
  int i;
  struct meta_context *meta_context;

  /* Unlike other can_FOO, this is not tracked in h->eflags, but is a
   * per-connection result. Find first ready connection, and assume
   * that all other connections will have the same set of contexts
   * (although not necessarily the same ordering or context ids).
   */
  for (i = 0; i < h->multi_conn; ++i) {
    if (!nbd_unlocked_aio_is_created (h->conns[i]) &&
        !nbd_unlocked_aio_is_connecting (h->conns[i])) {
      conn = h->conns[i];
      break;
    }
  }

  if (conn == NULL) {
    set_error (ENOTCONN, "handshake is not yet complete");
    return -1;
  }

  for (meta_context = conn->meta_contexts;
       meta_context;
       meta_context = meta_context->next)
    if (strcmp (meta_context->name, name) == 0)
      return 1;
  return 0;
}

int64_t
nbd_unlocked_get_size (struct nbd_handle *h)
{
  /* exportsize is only valid when we've read both the eflags and the
   * exportsize.  See comment in lib/internal.h.
   */
  if (h->eflags == 0) {
    set_error (EINVAL, "server has not returned export size, "
               "you need to connect to the server first");
    return -1;
  }

  return h->exportsize;
}
