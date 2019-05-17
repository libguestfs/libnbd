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
  conn->sbuf.option.version = htobe64 (NBD_NEW_VERSION);
  conn->sbuf.option.option = htobe32 (NBD_OPT_GO);
  conn->sbuf.option.optlen =
    htobe32 (/* exportnamelen */ 4 + strlen (h->export_name) + /* nrinfos */ 2);
  conn->wbuf = &conn->sbuf;
  conn->wlen = sizeof conn->sbuf.option;
  SET_NEXT_STATE (%SEND);
  return 0;

 NEWSTYLE.OPT_GO.SEND:
  const uint32_t exportnamelen = strlen (h->export_name);

  switch (send_from_wbuf (conn)) {
  case -1: SET_NEXT_STATE (%.DEAD); return -1;
  case 0:
    conn->sbuf.len = htobe32 (exportnamelen);
    conn->wbuf = &conn->sbuf;
    conn->wlen = 4;
    SET_NEXT_STATE (%SEND_EXPORTNAMELEN);
  }
  return 0;

 NEWSTYLE.OPT_GO.SEND_EXPORTNAMELEN:
  switch (send_from_wbuf (conn)) {
  case -1: SET_NEXT_STATE (%.DEAD); return -1;
  case 0:
    conn->wbuf = h->export_name;
    conn->wlen = strlen (h->export_name);
    SET_NEXT_STATE (%SEND_EXPORT);
  }
  return 0;

 NEWSTYLE.OPT_GO.SEND_EXPORT:
  switch (send_from_wbuf (conn)) {
  case -1: SET_NEXT_STATE (%.DEAD); return -1;
  case 0:
    conn->sbuf.nrinfos = 0;
    conn->wbuf = &conn->sbuf;
    conn->wlen = 2;
    SET_NEXT_STATE (%SEND_NRINFOS);
  }
  return 0;

 NEWSTYLE.OPT_GO.SEND_NRINFOS:
  switch (send_from_wbuf (conn)) {
  case -1: SET_NEXT_STATE (%.DEAD); return -1;
  case 0:
    conn->rbuf = &conn->sbuf;
    conn->rlen = sizeof conn->sbuf.or.option_reply;
    SET_NEXT_STATE (%RECV_REPLY);
  }
  return 0;

 NEWSTYLE.OPT_GO.RECV_REPLY:
  uint32_t len;
  const size_t maxpayload = sizeof conn->sbuf.or.payload;

  switch (recv_into_rbuf (conn)) {
  case -1: SET_NEXT_STATE (%.DEAD); return -1;
  case 0:
    /* Read the following payload if it is short enough to fit in the
     * static buffer.  If it's too long, skip it.
     */
    len = be32toh (conn->sbuf.or.option_reply.replylen);
    if (len <= maxpayload)
      conn->rbuf = &conn->sbuf.or.payload;
    else
      conn->rbuf = NULL;
    conn->rlen = len;
    SET_NEXT_STATE (%RECV_REPLY_PAYLOAD);
  }
  return 0;

 NEWSTYLE.OPT_GO.RECV_REPLY_PAYLOAD:
  switch (recv_into_rbuf (conn)) {
  case -1: SET_NEXT_STATE (%.DEAD); return -1;
  case 0:  SET_NEXT_STATE (%CHECK_REPLY);
  }
  return 0;

 NEWSTYLE.OPT_GO.CHECK_REPLY:
  uint64_t magic;
  uint32_t option;
  uint32_t reply;
  uint32_t len;
  const size_t maxpayload = sizeof conn->sbuf.or.payload;

  magic = be64toh (conn->sbuf.or.option_reply.magic);
  option = be32toh (conn->sbuf.or.option_reply.option);
  reply = be32toh (conn->sbuf.or.option_reply.reply);
  len = be32toh (conn->sbuf.or.option_reply.replylen);
  if (magic != NBD_REP_MAGIC || option != NBD_OPT_GO) {
    SET_NEXT_STATE (%.DEAD);
    set_error (0, "handshake: invalid option reply magic or option");
    return -1;
  }
  switch (reply) {
  case NBD_REP_ACK:
    SET_NEXT_STATE (%.READY);
    return 0;
  case NBD_REP_INFO:
    if (len <= maxpayload /* see RECV_NEWSTYLE_OPT_GO_REPLY */) {
      if (len >= sizeof conn->sbuf.or.payload.export) {
        if (be16toh (conn->sbuf.or.payload.export.info) == NBD_INFO_EXPORT) {
          conn->h->exportsize =
            be64toh (conn->sbuf.or.payload.export.exportsize);
          conn->h->eflags = be16toh (conn->sbuf.or.payload.export.eflags);
          debug (conn->h, "exportsize: %" PRIu64 " eflags: 0x%" PRIx16,
                 conn->h->exportsize, conn->h->eflags);
          if (conn->h->eflags == 0) {
            SET_NEXT_STATE (%.DEAD);
            set_error (EINVAL, "handshake: invalid eflags == 0 from server");
            return -1;
          }
        }
      }
    }
    /* ... else ignore the payload. */
    /* Server is allowed to send any number of NBD_REP_INFO, read next one. */
    conn->rbuf = &conn->sbuf;
    conn->rlen = sizeof (conn->sbuf.or.option_reply);
    SET_NEXT_STATE (%RECV_REPLY);
    return 0;
  case NBD_REP_ERR_UNSUP:
    /* XXX fall back to NBD_OPT_EXPORT_NAME */
    SET_NEXT_STATE (%.DEAD);
    set_error (0, "handshake: server does not support NBD_OPT_GO");
    return -1;
  default:
    SET_NEXT_STATE (%.DEAD);
    set_error (0, "handshake: unknown reply from NBD_OPT_GO: 0x%" PRIx32,
               reply);
    return -1;
  }

} /* END STATE MACHINE */
