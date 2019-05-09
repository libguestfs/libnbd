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

int64_t
nbd_unlocked_get_size (struct nbd_handle *h)
{
  /* exportsize is only valid when we're read both the eflags and the
   * exportsize.  See comment in lib/internal.h.
   */
  if (h->eflags == 0) {
    set_error (EINVAL, "server has not returned export size, "
               "you need to connect to the server first");
    return -1;
  }

  return h->exportsize;
}
