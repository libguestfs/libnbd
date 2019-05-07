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

/* Issue a read command on any connection and wait for the reply.  For
 * multi-conn this uses a simple round-robin.
 */
int
nbd_unlocked_pread (struct nbd_handle *h, void *buf,
                    size_t count, uint64_t offset)
{
  struct nbd_connection *conn = h->conns[h->unique % h->multi_conn];
  int64_t ch;

  ch = nbd_unlocked_aio_pread (conn, buf, count, offset);
  if (ch == -1)
    return -1;

  while (!nbd_unlocked_aio_command_completed (conn, ch)) {
    if (nbd_unlocked_poll (h, -1) == -1)
      return -1;
  }

  return 0;
}

/* Issue a write command on any connection and wait for the reply.
 * For multi-conn this uses a simple round-robin.
 */
int
nbd_unlocked_pwrite (struct nbd_handle *h, const void *buf,
                     size_t count, uint64_t offset)
{
  struct nbd_connection *conn = h->conns[h->unique % h->multi_conn];
  int64_t ch;

  ch = nbd_unlocked_aio_pwrite (conn, buf, count, offset);
  if (ch == -1)
    return -1;

  while (!nbd_unlocked_aio_command_completed (conn, ch)) {
    if (nbd_poll (h, -1) == -1)
      return -1;
  }

  return 0;
}

int64_t
nbd_unlocked_aio_pread (struct nbd_connection *conn, void *buf,
                        size_t count, uint64_t offset)
{
  struct command_in_flight *cmd;

  /* XXX CHECK COUNT NOT TOO LARGE! */

  cmd = malloc (sizeof *cmd);
  if (cmd == NULL) {
    set_error (errno, "malloc");
    return -1;
  }
  cmd->flags = 0;
  cmd->type = NBD_CMD_READ;
  cmd->handle = conn->h->unique++;
  cmd->offset = offset;
  cmd->count = count;
  cmd->data = buf;

  cmd->next = conn->cmds_to_issue;
  conn->cmds_to_issue = cmd;
  if (nbd_internal_run (conn->h, conn, cmd_issue) == -1)
    return -1;

  return cmd->handle;
}

int64_t
nbd_unlocked_aio_pwrite (struct nbd_connection *conn, const void *buf,
                         size_t count, uint64_t offset)
{
  struct command_in_flight *cmd;

  /* XXX CHECK COUNT NOT TOO LARGE! */
  /* XXX FUA FLAG */

  cmd = malloc (sizeof *cmd);
  if (cmd == NULL) {
    set_error (errno, "malloc");
    return -1;
  }
  cmd->flags = 0;
  cmd->type = NBD_CMD_WRITE;
  cmd->handle = conn->h->unique++;
  cmd->offset = offset;
  cmd->count = count;
  cmd->data = (void *) buf;

  cmd->next = conn->cmds_to_issue;
  conn->cmds_to_issue = cmd;
  if (nbd_internal_run (conn->h, conn, cmd_issue) == -1)
    return -1;

  return cmd->handle;
}
