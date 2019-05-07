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

int
nbd_unlocked_shutdown (struct nbd_handle *h)
{
  size_t i;

  for (i = 0; i < h->multi_conn; ++i) {
    if (nbd_unlocked_aio_is_ready (h->conns[i])) {
      if (nbd_unlocked_aio_disconnect (h->conns[i]) == -1)
        return -1;
    }
  }

  /* Wait until all sockets are closed or dead. */
  for (;;) {
    bool finished = true;

    for (i = 0; i < h->multi_conn; ++i) {
      if (!nbd_unlocked_aio_is_closed (h->conns[i]) &&
          !nbd_unlocked_aio_is_dead (h->conns[i]))
        finished = false;
    }

    if (finished)
      break;

    if (nbd_unlocked_poll (h, -1) == -1)
      return -1;
  }

  return 0;
}

int
nbd_unlocked_aio_disconnect (struct nbd_connection *conn)
{
  struct command_in_flight *cmd;

  cmd = malloc (sizeof *cmd);
  if (cmd == NULL) {
    set_error (errno, "malloc");
    return -1;
  }
  cmd->flags = 0;
  cmd->type = NBD_CMD_DISC;
  cmd->handle = conn->h->unique++;
  cmd->offset = 0;
  cmd->count = 0;
  cmd->data = NULL;

  cmd->next = conn->cmds_to_issue;
  conn->cmds_to_issue = cmd;

  /* This will leave the command on the in-flight list.  Is this a
   * problem?  Probably it isn't.  If it is, we could add a flag to
   * the command struct to tell SEND_REQUEST not to add it to the
   * in-flight list.
   */
  return nbd_internal_run (conn->h, conn, cmd_issue);
}
