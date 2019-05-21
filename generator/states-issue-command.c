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

/* State machine for issuing commands (requests) to the server. */

/* STATE MACHINE */ {
 ISSUE_COMMAND.START:
  struct command_in_flight *cmd;

  assert (conn->cmds_to_issue != NULL);
  cmd = conn->cmds_to_issue;

  /* Were we interrupted by reading a reply to an earlier command? */
  if (conn->wlen) {
    if (conn->in_write_payload)
      SET_NEXT_STATE(%SEND_WRITE_PAYLOAD);
    else
      SET_NEXT_STATE(%SEND_REQUEST);
    return 0;
  }

  conn->request.magic = htobe32 (NBD_REQUEST_MAGIC);
  conn->request.flags = htobe16 (cmd->flags);
  conn->request.type = htobe16 (cmd->type);
  conn->request.handle = htobe64 (cmd->handle);
  conn->request.offset = htobe64 (cmd->offset);
  conn->request.count = htobe32 ((uint32_t) cmd->count);
  conn->wbuf = &conn->request;
  conn->wlen = sizeof (conn->request);
  SET_NEXT_STATE (%SEND_REQUEST);
  return 0;

 ISSUE_COMMAND.SEND_REQUEST:
  switch (send_from_wbuf (conn)) {
  case -1: SET_NEXT_STATE (%.DEAD); return -1;
  case 0:  SET_NEXT_STATE (%PREPARE_WRITE_PAYLOAD);
  }
  return 0;

 ISSUE_COMMAND.PAUSE_SEND_REQUEST:
  assert (conn->wlen);
  assert (conn->cmds_to_issue != NULL);
  conn->in_write_payload = false;
  SET_NEXT_STATE (%^REPLY.START);
  return 0;

 ISSUE_COMMAND.PREPARE_WRITE_PAYLOAD:
  struct command_in_flight *cmd;

  assert (conn->cmds_to_issue != NULL);
  cmd = conn->cmds_to_issue;
  assert (cmd->handle == be64toh (conn->request.handle));
  if (cmd->type == NBD_CMD_WRITE) {
    conn->wbuf = cmd->data;
    conn->wlen = cmd->count;
    SET_NEXT_STATE (%SEND_WRITE_PAYLOAD);
  }
  else
    SET_NEXT_STATE (%FINISH);
  return 0;

 ISSUE_COMMAND.SEND_WRITE_PAYLOAD:
  switch (send_from_wbuf (conn)) {
  case -1: SET_NEXT_STATE (%.DEAD); return -1;
  case 0:  SET_NEXT_STATE (%FINISH);
  }
  return 0;

 ISSUE_COMMAND.PAUSE_WRITE_PAYLOAD:
  assert (conn->wlen);
  assert (conn->cmds_to_issue != NULL);
  conn->in_write_payload = true;
  SET_NEXT_STATE (%^REPLY.START);
  return 0;

 ISSUE_COMMAND.FINISH:
  struct command_in_flight *cmd;

  assert (!conn->wlen);
  assert (conn->cmds_to_issue != NULL);
  cmd = conn->cmds_to_issue;
  assert (cmd->handle == be64toh (conn->request.handle));
  conn->cmds_to_issue = cmd->next;
  cmd->next = conn->cmds_in_flight;
  conn->cmds_in_flight = cmd;
  SET_NEXT_STATE (%.READY);
  return 0;

} /* END STATE MACHINE */
