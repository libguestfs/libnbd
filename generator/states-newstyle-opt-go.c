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

/* State machine for ending fixed newstyle handshake with NBD_OPT_GO. */

/* STATE MACHINE */ {
 NEWSTYLE.OPT_GO.START:
  h->sbuf.option.version = htobe64 (NBD_NEW_VERSION);
  h->sbuf.option.option = htobe32 (NBD_OPT_GO);
  h->sbuf.option.optlen =
    htobe32 (/* exportnamelen */ 4 + strlen (h->export_name) + /* nrinfos */ 2);
  h->wbuf = &h->sbuf;
  h->wlen = sizeof h->sbuf.option;
  h->wflags = MSG_MORE;
  SET_NEXT_STATE (%SEND);
  return 0;

 NEWSTYLE.OPT_GO.SEND:
  const uint32_t exportnamelen = strlen (h->export_name);

  switch (send_from_wbuf (h)) {
  case -1: SET_NEXT_STATE (%.DEAD); return -1;
  case 0:
    h->sbuf.len = htobe32 (exportnamelen);
    h->wbuf = &h->sbuf;
    h->wlen = 4;
    h->wflags = MSG_MORE;
    SET_NEXT_STATE (%SEND_EXPORTNAMELEN);
  }
  return 0;

 NEWSTYLE.OPT_GO.SEND_EXPORTNAMELEN:
  switch (send_from_wbuf (h)) {
  case -1: SET_NEXT_STATE (%.DEAD); return -1;
  case 0:
    h->wbuf = h->export_name;
    h->wlen = strlen (h->export_name);
    h->wflags = MSG_MORE;
    SET_NEXT_STATE (%SEND_EXPORT);
  }
  return 0;

 NEWSTYLE.OPT_GO.SEND_EXPORT:
  switch (send_from_wbuf (h)) {
  case -1: SET_NEXT_STATE (%.DEAD); return -1;
  case 0:
    h->sbuf.nrinfos = 0;
    h->wbuf = &h->sbuf;
    h->wlen = 2;
    SET_NEXT_STATE (%SEND_NRINFOS);
  }
  return 0;

 NEWSTYLE.OPT_GO.SEND_NRINFOS:
  switch (send_from_wbuf (h)) {
  case -1: SET_NEXT_STATE (%.DEAD); return -1;
  case 0:
    h->rbuf = &h->sbuf;
    h->rlen = sizeof h->sbuf.or.option_reply;
    SET_NEXT_STATE (%RECV_REPLY);
  }
  return 0;

 NEWSTYLE.OPT_GO.RECV_REPLY:
  switch (recv_into_rbuf (h)) {
  case -1: SET_NEXT_STATE (%.DEAD); return -1;
  case 0:
    if (prepare_for_reply_payload (h, NBD_OPT_GO) == -1) {
      SET_NEXT_STATE (%.DEAD);
      return -1;
    }
    SET_NEXT_STATE (%RECV_REPLY_PAYLOAD);
  }
  return 0;

 NEWSTYLE.OPT_GO.RECV_REPLY_PAYLOAD:
  switch (recv_into_rbuf (h)) {
  case -1: SET_NEXT_STATE (%.DEAD); return -1;
  case 0:  SET_NEXT_STATE (%CHECK_REPLY);
  }
  return 0;

 NEWSTYLE.OPT_GO.CHECK_REPLY:
  uint32_t reply;
  uint32_t len;
  const size_t maxpayload = sizeof h->sbuf.or.payload;
  uint16_t info;
  uint64_t exportsize;
  uint16_t eflags;

  reply = be32toh (h->sbuf.or.option_reply.reply);
  len = be32toh (h->sbuf.or.option_reply.replylen);

  switch (reply) {
  case NBD_REP_ACK:
    SET_NEXT_STATE (%.READY);
    return 0;
  case NBD_REP_INFO:
    if (len > maxpayload /* see RECV_NEWSTYLE_OPT_GO_REPLY */)
      debug (h, "skipping large NBD_REP_INFO");
    else {
      assert (len >= sizeof h->sbuf.or.payload.export.info);
      info = be16toh (h->sbuf.or.payload.export.info);
      switch (info) {
      case NBD_INFO_EXPORT:
        if (len != sizeof h->sbuf.or.payload.export) {
          SET_NEXT_STATE (%.DEAD);
          set_error (0, "handshake: incorrect NBD_INFO_EXPORT option reply length");
          return -1;
        }
        exportsize = be64toh (h->sbuf.or.payload.export.exportsize);
        eflags = be16toh (h->sbuf.or.payload.export.eflags);
        if (nbd_internal_set_size_and_flags (h, exportsize, eflags) == -1) {
          SET_NEXT_STATE (%.DEAD);
          return -1;
        }
        break;
      default:
        /* XXX Handle other info types, like NBD_INFO_BLOCK_SIZE */
        debug (h, "skipping unknown NBD_REP_INFO type %d",
               be16toh (h->sbuf.or.payload.export.info));
        break;
      }
    }
    /* Server is allowed to send any number of NBD_REP_INFO, read next one. */
    h->rbuf = &h->sbuf;
    h->rlen = sizeof (h->sbuf.or.option_reply);
    SET_NEXT_STATE (%RECV_REPLY);
    return 0;
  case NBD_REP_ERR_UNSUP:
    debug (h, "server is confused by NBD_OPT_GO, continuing anyway");
    SET_NEXT_STATE (%^OPT_EXPORT_NAME.START);
    return 0;
  default:
    if (handle_reply_error (h) == 0)
      set_error (0, "handshake: unknown reply from NBD_OPT_GO: 0x%" PRIx32,
                 reply);
    SET_NEXT_STATE (%.DEAD);
    return -1;
  }

} /* END STATE MACHINE */
