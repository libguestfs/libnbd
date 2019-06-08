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
  uint32_t len;
  const size_t maxpayload = sizeof h->sbuf.or.payload;

  switch (recv_into_rbuf (h)) {
  case -1: SET_NEXT_STATE (%.DEAD); return -1;
  case 0:
    /* Read the following payload if it is short enough to fit in the
     * static buffer.  If it's too long, skip it.
     */
    len = be32toh (h->sbuf.or.option_reply.replylen);
    if (len <= maxpayload)
      h->rbuf = &h->sbuf.or.payload;
    else
      h->rbuf = NULL;
    h->rlen = len;
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
  uint64_t magic;
  uint32_t option;
  uint32_t reply;
  uint32_t len;
  const size_t maxpayload = sizeof h->sbuf.or.payload;
  uint64_t exportsize;
  uint16_t eflags;

  magic = be64toh (h->sbuf.or.option_reply.magic);
  option = be32toh (h->sbuf.or.option_reply.option);
  reply = be32toh (h->sbuf.or.option_reply.reply);
  len = be32toh (h->sbuf.or.option_reply.replylen);
  if (magic != NBD_REP_MAGIC || option != NBD_OPT_GO) {
    SET_NEXT_STATE (%.DEAD);
    set_error (0, "handshake: invalid option reply magic or option");
    return -1;
  }
  switch (reply) {
  case NBD_REP_ACK:
    if (len != 0) {
      SET_NEXT_STATE (%.DEAD);
      set_error (0, "handshake: invalid option reply length");
      return -1;
    }
    SET_NEXT_STATE (%.READY);
    return 0;
  case NBD_REP_INFO:
    if (len <= maxpayload /* see RECV_NEWSTYLE_OPT_GO_REPLY */) {
      if (len >= sizeof h->sbuf.or.payload.export) {
        if (be16toh (h->sbuf.or.payload.export.info) == NBD_INFO_EXPORT) {
          exportsize = be64toh (h->sbuf.or.payload.export.exportsize);
          eflags = be16toh (h->sbuf.or.payload.export.eflags);
          if (nbd_internal_set_size_and_flags (h,
                                               exportsize, eflags) == -1) {
            SET_NEXT_STATE (%.DEAD);
            return -1;
          }
        }
      }
    }
    /* ... else ignore the payload. */
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
    SET_NEXT_STATE (%.DEAD);
    set_error (0, "handshake: unknown reply from NBD_OPT_GO: 0x%" PRIx32,
               reply);
    return -1;
  }

} /* END STATE MACHINE */
