/* nbd client library in userspace: state machine
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

/* State machine for parsing simple replies from the server. */

/* STATE MACHINE */ {
 REPLY.SIMPLE_REPLY.START:
  struct command_in_flight *cmd;
  uint32_t error;
  uint64_t handle;

  error = be32toh (conn->sbuf.simple_reply.error);
  handle = be64toh (conn->sbuf.simple_reply.handle);

  /* Find the command amongst the commands in flight. */
  for (cmd = conn->cmds_in_flight; cmd != NULL; cmd = cmd->next) {
    if (cmd->handle == handle)
      break;
  }
  if (cmd == NULL) {
    SET_NEXT_STATE (%.READY);
    set_error (0, "no matching handle found for server reply, "
               "this is probably a bug in the server");
    return -1;
  }

  cmd->error = error;
  if (cmd->error == 0 && cmd->type == NBD_CMD_READ) {
    conn->rbuf = cmd->data;
    conn->rlen = cmd->count;
    SET_NEXT_STATE (%RECV_READ_PAYLOAD);
  }
  else {
    SET_NEXT_STATE (%^FINISH_COMMAND);
  }
  return 0;

 REPLY.SIMPLE_REPLY.RECV_READ_PAYLOAD:
  switch (recv_into_rbuf (conn)) {
  case -1: SET_NEXT_STATE (%.DEAD); return -1;
  case 0:  SET_NEXT_STATE (%^FINISH_COMMAND);
  }
  return 0;

} /* END STATE MACHINE */
