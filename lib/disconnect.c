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
#include <inttypes.h>

#include "internal.h"

int
nbd_unlocked_shutdown (struct nbd_handle *h, uint32_t flags)
{
  if (flags != 0) {
    set_error (EINVAL, "invalid flag: %" PRIu32, flags);
    return -1;
  }

  if (!h->disconnect_request &&
      (nbd_internal_is_state_ready (get_next_state (h)) ||
       nbd_internal_is_state_processing (get_next_state (h)))) {
    if (nbd_unlocked_aio_disconnect (h, 0) == -1)
      return -1;
  }

  while (!nbd_internal_is_state_closed (get_next_state (h)) &&
         !nbd_internal_is_state_dead (get_next_state (h))) {
    if (nbd_unlocked_poll (h, -1) == -1)
      return -1;
  }

  return 0;
}

int
nbd_unlocked_aio_disconnect (struct nbd_handle *h, uint32_t flags)
{
  int64_t id;

  if (flags != 0) {
    set_error (EINVAL, "invalid flag: %" PRIu32, flags);
    return -1;
  }

  id = nbd_internal_command_common (h, 0, NBD_CMD_DISC, 0, 0, NULL, NULL);
  if (id == -1)
    return -1;
  h->disconnect_request = true;

  /* As the server does not reply to this command, it is left
   * in-flight until the cleanup performed when moving to CLOSED or
   * DEAD.  We don't return a handle to the user, and thus also
   * special case things so that the user cannot request the status of
   * this command during aio_[peek_]command_completed.
   */
  return 0;
}
