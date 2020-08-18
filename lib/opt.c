/* NBD client library in userspace
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

#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>

#include "internal.h"

/* Internal function which frees an option with callback. */
void
nbd_internal_free_option (struct nbd_handle *h)
{
  FREE_CALLBACK (h->opt_completion);
}

int
nbd_unlocked_set_opt_mode (struct nbd_handle *h, bool value)
{
  h->opt_mode = value;
  return 0;
}

/* NB: may_set_error = false. */
int
nbd_unlocked_get_opt_mode (struct nbd_handle *h)
{
  return h->opt_mode;
}

static int
wait_for_option (struct nbd_handle *h)
{
  while (nbd_internal_is_state_connecting (get_next_state (h))) {
    if (nbd_unlocked_poll (h, -1) == -1)
      return -1;
  }

  return 0;
}

static int
go_complete (void *opaque, int *err)
{
  int *i = opaque;
  *i = *err;
  return 0;
}

/* Issue NBD_OPT_GO (or NBD_OPT_EXPORT_NAME) and wait for the reply. */
int
nbd_unlocked_opt_go (struct nbd_handle *h)
{
  int err;
  nbd_completion_callback c = { .callback = go_complete, .user_data = &err };
  int r = nbd_unlocked_aio_opt_go (h, c);

  if (r == -1)
    return r;

  r = wait_for_option (h);
  if (r == 0 && err) {
    assert (nbd_internal_is_state_negotiating (get_next_state (h)));
    set_error (err, "server replied with error to opt_go request");
    return -1;
  }
  if (r == 0)
    assert (nbd_internal_is_state_ready (get_next_state (h)));
  return r;
}

/* Issue NBD_OPT_ABORT and wait for the state change. */
int
nbd_unlocked_opt_abort (struct nbd_handle *h)
{
  int r = nbd_unlocked_aio_opt_abort (h);

  if (r == -1)
    return r;

  return wait_for_option (h);
}

/* Issue NBD_OPT_GO (or NBD_OPT_EXPORT_NAME) without waiting. */
int
nbd_unlocked_aio_opt_go (struct nbd_handle *h,
                         nbd_completion_callback complete)
{
  h->opt_current = NBD_OPT_GO;
  h->opt_completion = complete;

  if (nbd_internal_run (h, cmd_issue) == -1)
    debug (h, "option queued, ignoring state machine failure");
  return 0;
}

/* Issue NBD_OPT_ABORT without waiting. */
int
nbd_unlocked_aio_opt_abort (struct nbd_handle *h)
{
  h->opt_current = NBD_OPT_ABORT;

  if (nbd_internal_run (h, cmd_issue) == -1)
    debug (h, "option queued, ignoring state machine failure");
  return 0;
}
