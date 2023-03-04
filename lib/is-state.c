/* NBD client library in userspace
 * Copyright Red Hat
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

/* Internal functions to test state or groups of states. */

bool
nbd_internal_is_state_created (enum state state)
{
  return state == STATE_START;
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

bool
nbd_internal_is_state_connecting (enum state state)
{
  enum state_group group = nbd_internal_state_group (state);

  return is_connecting_group (group);
}

bool
nbd_internal_is_state_negotiating (enum state state)
{
  return state == STATE_NEGOTIATING;
}

bool
nbd_internal_is_state_ready (enum state state)
{
  return state == STATE_READY;
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

bool
nbd_internal_is_state_processing (enum state state)
{
  enum state_group group = nbd_internal_state_group (state);

  return is_processing_group (group);
}

bool
nbd_internal_is_state_dead (enum state state)
{
  return state == STATE_DEAD;
}

bool
nbd_internal_is_state_closed (enum state state)
{
  return state == STATE_CLOSED;
}

/* The nbd_unlocked_aio_is_* and nbd_unlocked_aio_get_direction calls are
 * the public APIs for reading the state of the handle.
 *
 * They all have: is_locked = false, may_set_error = false.
 *
 * They all read the public state, not the real state.  Therefore you
 * SHOULD NOT call these functions from elsewhere in the library (use
 * nbd_internal_is_* and nbd_internal_aio_get_direction instead).
 */

int
nbd_unlocked_aio_is_created (struct nbd_handle *h)
{
  return nbd_internal_is_state_created (get_public_state (h));
}

int
nbd_unlocked_aio_is_connecting (struct nbd_handle *h)
{
  return nbd_internal_is_state_connecting (get_public_state (h));
}

int
nbd_unlocked_aio_is_negotiating (struct nbd_handle *h)
{
  return nbd_internal_is_state_negotiating (get_public_state (h));
}

int
nbd_unlocked_aio_is_ready (struct nbd_handle *h)
{
  return nbd_internal_is_state_ready (get_public_state (h));
}

int
nbd_unlocked_aio_is_processing (struct nbd_handle *h)
{
  return nbd_internal_is_state_processing (get_public_state (h));
}

int
nbd_unlocked_aio_is_dead (struct nbd_handle *h)
{
  return nbd_internal_is_state_dead (get_public_state (h));
}

int
nbd_unlocked_aio_is_closed (struct nbd_handle *h)
{
  return nbd_internal_is_state_closed (get_public_state (h));
}

unsigned
nbd_unlocked_aio_get_direction (struct nbd_handle *h)
{
  return nbd_internal_aio_get_direction (get_public_state (h));
}
