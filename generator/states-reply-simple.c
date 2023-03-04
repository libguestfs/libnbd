/* nbd client library in userspace: state machine
 * Copyright Red Hat
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

  error = be32toh (h->sbuf.simple_reply.error);

  if (cmd == NULL) {
    /* Unexpected reply.  If error was set or we have structured
     * replies, we know there should be no payload, so the next byte
     * on the wire (if any) will be another reply, and we can let
     * FINISH_COMMAND diagnose/ignore the server bug.  If not, we lack
     * context to know whether the server thinks it was responding to
     * NBD_CMD_READ, so it is safer to move to DEAD now than to risk
     * consuming a server's potential data payload as a reply stream
     * (even though we would be likely to produce a magic number
     * mismatch on the next pass that would also move us to DEAD).
     */
    if (error || h->structured_replies)
      SET_NEXT_STATE (%^FINISH_COMMAND);
    else {
      uint64_t cookie = be64toh (h->sbuf.simple_reply.handle);
      SET_NEXT_STATE (%.DEAD);
      set_error (EPROTO,
                 "no matching cookie %" PRIu64 " found for server reply, "
                 "this is probably a server bug", cookie);
    }
    return 0;
  }

  /* Although a server with structured replies negotiated is in error
   * for using a simple reply to NBD_CMD_READ, we can cope with the
   * packet, but diagnose it by failing the read with EPROTO.
   */
  if (cmd->type == NBD_CMD_READ && h->structured_replies) {
    debug (h, "server sent unexpected simple reply for read");
    if (cmd->error == 0)
      cmd->error = EPROTO;
  }

  error = nbd_internal_errno_of_nbd_error (error);
  if (cmd->error == 0)
    cmd->error = error;
  if (error == 0 && cmd->type == NBD_CMD_READ) {
    h->rbuf = cmd->data;
    h->rlen = cmd->count;
    cmd->data_seen += cmd->count;
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
      int error = cmd->error;

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
