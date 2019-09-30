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

/* State machine for negotiating NBD_OPT_STRUCTURED_REPLY. */

STATE_MACHINE {
 NEWSTYLE.OPT_STRUCTURED_REPLY.START:
  if (!h->request_sr) {
    SET_NEXT_STATE (%^OPT_SET_META_CONTEXT.START);
    return 0;
  }

  h->sbuf.option.version = htobe64 (NBD_NEW_VERSION);
  h->sbuf.option.option = htobe32 (NBD_OPT_STRUCTURED_REPLY);
  h->sbuf.option.optlen = htobe32 (0);
  h->wbuf = &h->sbuf;
  h->wlen = sizeof h->sbuf.option;
  SET_NEXT_STATE (%SEND);
  return 0;

 NEWSTYLE.OPT_STRUCTURED_REPLY.SEND:
  switch (send_from_wbuf (h)) {
  case -1: SET_NEXT_STATE (%.DEAD); return 0;
  case 0:
    h->rbuf = &h->sbuf;
    h->rlen = sizeof h->sbuf.or.option_reply;
    SET_NEXT_STATE (%RECV_REPLY);
  }
  return 0;

 NEWSTYLE.OPT_STRUCTURED_REPLY.RECV_REPLY:
  switch (recv_into_rbuf (h)) {
  case -1: SET_NEXT_STATE (%.DEAD); return 0;
  case 0:
    if (prepare_for_reply_payload (h, NBD_OPT_STRUCTURED_REPLY) == -1) {
      SET_NEXT_STATE (%.DEAD);
      return 0;
    }
    SET_NEXT_STATE (%RECV_REPLY_PAYLOAD);
  }
  return 0;

 NEWSTYLE.OPT_STRUCTURED_REPLY.RECV_REPLY_PAYLOAD:
  switch (recv_into_rbuf (h)) {
  case -1: SET_NEXT_STATE (%.DEAD); return 0;
  case 0:  SET_NEXT_STATE (%CHECK_REPLY);
  }
  return 0;

 NEWSTYLE.OPT_STRUCTURED_REPLY.CHECK_REPLY:
  uint32_t reply;

  reply = be32toh (h->sbuf.or.option_reply.reply);
  switch (reply) {
  case NBD_REP_ACK:
    debug (h, "negotiated structured replies on this connection");
    h->structured_replies = true;
    break;
  default:
    if (handle_reply_error (h) == -1) {
      SET_NEXT_STATE (%.DEAD);
      return 0;
    }

    debug (h, "structured replies are not supported by this server");
    h->structured_replies = false;
    break;
  }

  /* Next option. */
  SET_NEXT_STATE (%^OPT_SET_META_CONTEXT.START);
  return 0;

} /* END STATE MACHINE */
