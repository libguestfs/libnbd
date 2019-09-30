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

STATE_MACHINE {
 REPLY.SIMPLE_REPLY.START:
  struct command *cmd = h->reply_cmd;
  uint32_t error;
  uint64_t cookie;

  error = be32toh (h->sbuf.simple_reply.error);
  cookie = be64toh (h->sbuf.simple_reply.handle);

  assert (cmd);
  assert (cmd->cookie == cookie);

  if (cmd->type == NBD_CMD_READ && h->structured_replies) {
    set_error (0, "server sent unexpected simple reply for read");
    SET_NEXT_STATE(%.DEAD);
    return 0;
  }

  cmd->error = nbd_internal_errno_of_nbd_error (error);
  if (cmd->error == 0 && cmd->type == NBD_CMD_READ) {
    h->rbuf = cmd->data;
    h->rlen = cmd->count;
    cmd->data_seen = true;
    SET_NEXT_STATE (%RECV_READ_PAYLOAD);
  }
  else {
    SET_NEXT_STATE (%^FINISH_COMMAND);
  }
  return 0;

 REPLY.SIMPLE_REPLY.RECV_READ_PAYLOAD:
  struct command *cmd = h->reply_cmd;

  switch (recv_into_rbuf (h)) {
  case -1: SET_NEXT_STATE (%.DEAD); return 0;
  case 1:
    save_reply_state (h);
    SET_NEXT_STATE (%.READY);
    return 0;
  case 0:
    /* guaranteed by START */
    assert (cmd);
    if (CALLBACK_IS_NOT_NULL (cmd->cb.fn.chunk)) {
      int error = 0;

      assert (cmd->error == 0);
      if (CALL_CALLBACK (cmd->cb.fn.chunk,
                         cmd->data, cmd->count,
                         cmd->offset, LIBNBD_READ_DATA,
                         &error) == -1)
        cmd->error = error ? error : EPROTO;
    }

    SET_NEXT_STATE (%^FINISH_COMMAND);
  }
  return 0;

} /* END STATE MACHINE */
