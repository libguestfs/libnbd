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
#include <assert.h>

#include "internal.h"

/* Internal function which retires and frees a command. */
void
nbd_internal_retire_and_free_command (struct command *cmd)
{
  /* Free the callbacks. */
  if (cmd->type == NBD_CMD_BLOCK_STATUS)
    FREE_CALLBACK (cmd->cb.fn.extent);
  if (cmd->type == NBD_CMD_READ)
    FREE_CALLBACK (cmd->cb.fn.chunk);
  FREE_CALLBACK (cmd->cb.completion);

  free (cmd);
}

int
nbd_unlocked_aio_get_fd (struct nbd_handle *h)
{
  if (!h->sock) {
    set_error (EINVAL, "connection is not in a connected state");
    return -1;
  }
  return h->sock->ops->get_fd (h->sock);
}

int
nbd_unlocked_aio_notify_read (struct nbd_handle *h)
{
  return nbd_internal_run (h, notify_read);
}

int
nbd_unlocked_aio_notify_write (struct nbd_handle *h)
{
  return nbd_internal_run (h, notify_write);
}

int
nbd_unlocked_aio_command_completed (struct nbd_handle *h,
                                    uint64_t cookie)
{
  struct command *prev_cmd, *cmd;
  uint16_t type;
  uint32_t error;

  if (cookie < 1) {
    set_error (EINVAL, "invalid aio cookie %" PRId64, cookie);
    return -1;
  }

  /* Find the command amongst the completed commands. */
  for (cmd = h->cmds_done, prev_cmd = NULL;
       cmd != NULL;
       prev_cmd = cmd, cmd = cmd->next) {
    if (cmd->cookie == cookie)
      break;
  }
  if (!cmd)
    return 0;

  type = cmd->type;
  error = cmd->error;
  assert (cmd->type != NBD_CMD_DISC);
  /* The spec states that a 0-length read request is unspecified; but
   * it is easy enough to treat it as successful as an extension.
   */
  if (type == NBD_CMD_READ && !cmd->data_seen && cmd->count && !error)
    error = EIO;

  /* Retire it from the list and free it. */
  if (h->cmds_done_tail == cmd) {
    assert (cmd->next == NULL);
    h->cmds_done_tail = prev_cmd;
  }
  if (prev_cmd != NULL)
    prev_cmd->next = cmd->next;
  else
    h->cmds_done = cmd->next;

  nbd_internal_retire_and_free_command (cmd);

  /* If the command was successful, return true. */
  if (error == 0)
    return 1;

  /* The command failed, set an error indication and return an error. */
  set_error (error, "%s: command failed",
             nbd_internal_name_of_nbd_cmd (type));
  return -1;
}

int64_t
nbd_unlocked_aio_peek_command_completed (struct nbd_handle *h)
{
  if (h->cmds_done != NULL) {
    assert (h->cmds_done->type != NBD_CMD_DISC);
    return h->cmds_done->cookie;
  }

  if (h->cmds_in_flight != NULL || h->cmds_to_issue != NULL) {
    set_error (0, "no in-flight command has completed yet");
    return 0;
  }
  set_error (EINVAL, "no commands are in flight");
  return -1;
}

int
nbd_unlocked_aio_in_flight (struct nbd_handle *h)
{
  return h->in_flight;
}
