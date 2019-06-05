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

#include "internal.h"

/* NB: is_locked = false, may_set_error = false. */
int
nbd_unlocked_aio_is_created (struct nbd_handle *h)
{
  return h->state == STATE_START;
}

static int
is_connecting_group (enum state_group group)
{
  switch (group) {
  case GROUP_TOP:
    return 0;
  case GROUP_CONNECT:
  case GROUP_CONNECT_TCP:
  case GROUP_CONNECT_COMMAND:
  case GROUP_MAGIC:
  case GROUP_OLDSTYLE:
  case GROUP_NEWSTYLE:
    return 1;
  default:
    return is_connecting_group (nbd_internal_state_group_parent (group));
  }
}

/* NB: is_locked = false, may_set_error = false. */
int
nbd_unlocked_aio_is_connecting (struct nbd_handle *h)
{
  enum state_group group = nbd_internal_state_group (h->state);

  return is_connecting_group (group);
}

/* NB: is_locked = false, may_set_error = false. */
int
nbd_unlocked_aio_is_ready (struct nbd_handle *h)
{
  return h->state == STATE_READY;
}

static int
is_processing_group (enum state_group group)
{
  switch (group) {
  case GROUP_TOP:
    return 0;
  case GROUP_ISSUE_COMMAND:
  case GROUP_REPLY:
    return 1;
  default:
    return is_processing_group (nbd_internal_state_group_parent (group));
  }
}

/* NB: is_locked = false, may_set_error = false. */
int
nbd_unlocked_aio_is_processing (struct nbd_handle *h)
{
  enum state_group group = nbd_internal_state_group (h->state);

  return is_processing_group (group);
}

/* NB: is_locked = false, may_set_error = false. */
int
nbd_unlocked_aio_is_dead (struct nbd_handle *h)
{
  return h->state == STATE_DEAD;
}

/* NB: is_locked = false, may_set_error = false. */
int
nbd_unlocked_aio_is_closed (struct nbd_handle *h)
{
  return h->state == STATE_CLOSED;
}
