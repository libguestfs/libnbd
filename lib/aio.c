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
nbd_unlocked_aio_get_fd (struct nbd_connection *conn)
{
  return conn->fd;
}

int
nbd_unlocked_aio_notify_read (struct nbd_connection *conn)
{
  return nbd_internal_run (conn->h, conn, notify_read);
}

int
nbd_unlocked_aio_notify_write (struct nbd_connection *conn)
{
  return nbd_internal_run (conn->h, conn, notify_write);
}

int
nbd_unlocked_aio_is_ready (struct nbd_connection *conn)
{
  return conn->state == STATE_READY;
}

int
nbd_unlocked_aio_is_dead (struct nbd_connection *conn)
{
  return conn->state == STATE_DEAD;
}

int
nbd_unlocked_aio_command_completed (struct nbd_connection *conn,
                                    int64_t handle)
{
  struct command_in_flight *prev_cmd, *cmd;

  /* Find the command amongst the completed commands. */
  for (cmd = conn->cmds_done, prev_cmd = NULL;
       cmd != NULL;
       prev_cmd = cmd, cmd = cmd->next) {
    if (cmd->handle == handle)
      break;
  }
  if (!cmd)
    return 0;

  /* Retire it from the list and free it. */
  if (prev_cmd != NULL)
    prev_cmd->next = cmd->next;
  else
    conn->cmds_done = cmd->next;

  free (cmd);
  return 1;
}
