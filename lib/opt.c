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
#include <limits.h>
#include <errno.h>
#include <assert.h>

#include "internal.h"

/* Internal function which frees an option with callback. */
void
nbd_internal_free_option (struct nbd_handle *h)
{
  if (h->opt_current == NBD_OPT_LIST)
    FREE_CALLBACK (h->opt_cb.fn.list);
  FREE_CALLBACK (h->opt_cb.completion);
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

struct list_helper {
  int count;
  nbd_list_callback list;
  int err;
};
static int
list_visitor (void *opaque, const char *name, const char *description)
{
  struct list_helper *h = opaque;
  if (h->count < INT_MAX)
    h->count++;
  CALL_CALLBACK (h->list, name, description);
  return 0;
}
static int
list_complete (void *opaque, int *err)
{
  struct list_helper *h = opaque;
  h->err = *err;
  FREE_CALLBACK (h->list);
  return 0;
}

/* Issue NBD_OPT_LIST and wait for the reply. */
int
nbd_unlocked_opt_list (struct nbd_handle *h, nbd_list_callback list)
{
  struct list_helper s = { .list = list };
  nbd_list_callback l = { .callback = list_visitor, .user_data = &s };
  nbd_completion_callback c = { .callback = list_complete, .user_data = &s };

  if (nbd_unlocked_aio_opt_list (h, l, c) == -1)
    return -1;

  if (wait_for_option (h) == -1)
    return -1;
  if (s.err) {
    set_error (s.err, "server replied with error to list request");
    return -1;
  }
  return s.count;
}

/* Issue NBD_OPT_GO (or NBD_OPT_EXPORT_NAME) without waiting. */
int
nbd_unlocked_aio_opt_go (struct nbd_handle *h,
                         nbd_completion_callback complete)
{
  h->opt_current = NBD_OPT_GO;
  h->opt_cb.completion = complete;

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

/* Issue NBD_OPT_LIST without waiting. */
int
nbd_unlocked_aio_opt_list (struct nbd_handle *h, nbd_list_callback list,
                           nbd_completion_callback complete)
{
  if ((h->gflags & LIBNBD_HANDSHAKE_FLAG_FIXED_NEWSTYLE) == 0) {
    set_error (ENOTSUP, "server is not using fixed newstyle protocol");
    return -1;
  }

  assert (CALLBACK_IS_NULL (h->opt_cb.fn.list));
  h->opt_cb.fn.list = list;
  h->opt_cb.completion = complete;
  h->opt_current = NBD_OPT_LIST;
  if (nbd_internal_run (h, cmd_issue) == -1)
    debug (h, "option queued, ignoring state machine failure");
  return 0;
}
