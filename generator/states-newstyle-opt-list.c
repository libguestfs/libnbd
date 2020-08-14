/* nbd client library in userspace: state machine
 * Copyright (C) 2013-2020 Red Hat Inc.
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

/* State machine for sending NBD_OPT_LIST to list exports.
 *
 * This is only reached via nbd_opt_list during opt_mode.
 */

STATE_MACHINE {
 NEWSTYLE.OPT_LIST.START:
  assert (h->gflags & LIBNBD_HANDSHAKE_FLAG_FIXED_NEWSTYLE);
  assert (h->opt_mode && h->exports && !h->nr_exports);
  h->sbuf.option.version = htobe64 (NBD_NEW_VERSION);
  h->sbuf.option.option = htobe32 (NBD_OPT_LIST);
  h->sbuf.option.optlen = 0;
  h->wbuf = &h->sbuf;
  h->wlen = sizeof (h->sbuf.option);
  SET_NEXT_STATE (%SEND);
  return 0;

 NEWSTYLE.OPT_LIST.SEND:
  switch (send_from_wbuf (h)) {
  case -1: SET_NEXT_STATE (%.DEAD); return 0;
  case 0:
    h->rbuf = &h->sbuf;
    h->rlen = sizeof (h->sbuf.or.option_reply);
    SET_NEXT_STATE (%RECV_REPLY);
  }
  return 0;

 NEWSTYLE.OPT_LIST.RECV_REPLY:
  switch (recv_into_rbuf (h)) {
  case -1: SET_NEXT_STATE (%.DEAD); return 0;
  case 0:
    if (prepare_for_reply_payload (h, NBD_OPT_LIST) == -1) {
      SET_NEXT_STATE (%.DEAD);
      return 0;
    }
    SET_NEXT_STATE (%RECV_REPLY_PAYLOAD);
  }
  return 0;

 NEWSTYLE.OPT_LIST.RECV_REPLY_PAYLOAD:
  switch (recv_into_rbuf (h)) {
  case -1: SET_NEXT_STATE (%.DEAD); return 0;
  case 0:  SET_NEXT_STATE (%CHECK_REPLY);
  }
  return 0;

 NEWSTYLE.OPT_LIST.CHECK_REPLY:
  const size_t maxpayload = sizeof h->sbuf.or.payload.server;
  uint32_t reply;
  uint32_t len;
  uint32_t elen;
  struct export exp;
  struct export *new_exports;

  reply = be32toh (h->sbuf.or.option_reply.reply);
  len = be32toh (h->sbuf.or.option_reply.replylen);
  switch (reply) {
  case NBD_REP_SERVER:
    /* Got one export. */
    if (len > maxpayload)
      debug (h, "skipping too large export name reply");
    else {
      elen = be32toh (h->sbuf.or.payload.server.server.export_name_len);
      if (elen > len - 4 || elen > NBD_MAX_STRING ||
          len - 4 - elen > NBD_MAX_STRING) {
        set_error (0, "invalid export length");
        SET_NEXT_STATE (%.DEAD);
        return 0;
      }
      /* Copy the export name and description to the handle list. */
      exp.name = strndup (h->sbuf.or.payload.server.str, elen);
      if (exp.name == NULL) {
        set_error (errno, "strdup");
        SET_NEXT_STATE (%.DEAD);
        return 0;
      }
      exp.description = strndup (h->sbuf.or.payload.server.str + elen,
                                 len - 4 - elen);
      if (exp.description == NULL) {
        set_error (errno, "strdup");
        free (exp.name);
        SET_NEXT_STATE (%.DEAD);
        return 0;
      }
      new_exports = realloc (h->exports,
                             sizeof (*new_exports) * (h->nr_exports+1));
      if (new_exports == NULL) {
        set_error (errno, "strdup");
        SET_NEXT_STATE (%.DEAD);
        free (exp.name);
        free (exp.description);
        return 0;
      }
      h->exports = new_exports;
      h->exports[h->nr_exports++] = exp;
    }

    /* Just limit this so we don't receive unlimited amounts
     * of data from the server.  Note each export name can be
     * up to 4K.
     */
    if (h->nr_exports > 10000) {
      set_error (0, "too many export names sent by the server");
      SET_NEXT_STATE (%.DEAD);
      return 0;
    }

    /* Wait for more replies. */
    h->rbuf = &h->sbuf;
    h->rlen = sizeof (h->sbuf.or.option_reply);
    SET_NEXT_STATE (%RECV_REPLY);
    return 0;

  case NBD_REP_ACK:
    /* Finished receiving the list. */
    SET_NEXT_STATE (%.NEGOTIATING);
    return 0;

  default:
    if (handle_reply_error (h) == -1) {
      SET_NEXT_STATE (%.DEAD);
      return 0;
    }
    set_error (ENOTSUP, "unexpected response, possibly the server does not "
               "support listing exports");
    SET_NEXT_STATE (%.NEGOTIATING);
    return 0;
  }
  return 0;

} /* END STATE MACHINE */
