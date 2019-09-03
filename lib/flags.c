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
#include <stdint.h>
#include <inttypes.h>
#include <errno.h>

#include "internal.h"

/* Set the export size and eflags on the handle, validating them.
 * This is called from the state machine when either the newstyle or
 * oldstyle negotiation reaches the point that these are available.
 */
int
nbd_internal_set_size_and_flags (struct nbd_handle *h,
                                 uint64_t exportsize, uint16_t eflags)
{
  debug (h, "exportsize: %" PRIu64 " eflags: 0x%" PRIx16, exportsize, eflags);

  if (eflags == 0) {
    set_error (EINVAL, "handshake: invalid eflags == 0 from server");
    return -1;
  }

  if (eflags & NBD_FLAG_SEND_DF && !h->structured_replies) {
    debug (h, "server lacks structured replies, ignoring claim of df");
    eflags &= ~NBD_FLAG_SEND_DF;
  }

  if (eflags & NBD_FLAG_SEND_FAST_ZERO &&
      !(eflags & NBD_FLAG_SEND_WRITE_ZEROES)) {
    debug (h, "server lacks write zeroes, ignoring claim of fast zero");
    eflags &= ~NBD_FLAG_SEND_FAST_ZERO;
  }

  h->exportsize = exportsize;
  h->eflags = eflags;
  return 0;
}

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
nbd_unlocked_is_read_only (struct nbd_handle *h)
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
nbd_unlocked_can_fast_zero (struct nbd_handle *h)
{
  return get_flag (h, NBD_FLAG_SEND_FAST_ZERO);
}

int
nbd_unlocked_can_df (struct nbd_handle *h)
{
  return get_flag (h, NBD_FLAG_SEND_DF);
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
  struct meta_context *meta_context;

  for (meta_context = h->meta_contexts;
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
