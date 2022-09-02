/* nbd client library in userspace: state machine
 * Copyright (C) 2013-2022 Red Hat Inc.
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
  assert (h->opt_mode && h->opt_current == NBD_OPT_LIST);
  assert (CALLBACK_IS_NOT_NULL (h->opt_cb.fn.list));
  h->sbuf.option.version = htobe64 (NBD_NEW_VERSION);
  h->sbuf.option.option = htobe32 (NBD_OPT_LIST);
  h->sbuf.option.optlen = 0;
  h->chunks_sent++;
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
  const char *name;
  const char *desc;
  char *tmp;
  int err;

  reply = be32toh (h->sbuf.or.option_reply.reply);
  len = be32toh (h->sbuf.or.option_reply.replylen);
  switch (reply) {
  case NBD_REP_SERVER:
    /* Got one export. */
    if (len >= maxpayload)
      debug (h, "skipping too large export name reply");
    else {
      /* server.str is oversized for trailing NUL byte convenience */
      h->sbuf.or.payload.server.str[len - 4] = '\0';
      elen = be32toh (h->sbuf.or.payload.server.server.export_name_len);
      if (elen > len - 4 || elen > NBD_MAX_STRING ||
          len - 4 - elen > NBD_MAX_STRING) {
        set_error (0, "invalid export length");
        SET_NEXT_STATE (%.DEAD);
        return 0;
      }
      if (elen == len + 4) {
        tmp = NULL;
        name = h->sbuf.or.payload.server.str;
        desc = "";
      }
      else {
        tmp = strndup (h->sbuf.or.payload.server.str, elen);
        if (tmp == NULL) {
          set_error (errno, "strdup");
          SET_NEXT_STATE (%.DEAD);
          return 0;
        }
        name = tmp;
        desc = h->sbuf.or.payload.server.str + elen;
      }
      CALL_CALLBACK (h->opt_cb.fn.list, name, desc);
      free (tmp);
    }

    /* Wait for more replies. */
    h->rbuf = &h->sbuf;
    h->rlen = sizeof (h->sbuf.or.option_reply);
    SET_NEXT_STATE (%RECV_REPLY);
    return 0;

  case NBD_REP_ACK:
    /* Finished receiving the list. */
    err = 0;
    break;

  default:
    if (handle_reply_error (h) == -1) {
      SET_NEXT_STATE (%.DEAD);
      return 0;
    }
    err = ENOTSUP;
    set_error (err, "unexpected response, possibly the server does not "
               "support listing exports");
    break;
  }

  CALL_CALLBACK (h->opt_cb.completion, &err);
  nbd_internal_free_option (h);
  SET_NEXT_STATE (%.NEGOTIATING);
  return 0;

} /* END STATE MACHINE */
