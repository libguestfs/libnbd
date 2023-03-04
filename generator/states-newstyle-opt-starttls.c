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

/* State machine for sending NBD_OPT_STARTTLS. */

STATE_MACHINE {
 NEWSTYLE.OPT_STARTTLS.START:
  assert (h->gflags & LIBNBD_HANDSHAKE_FLAG_FIXED_NEWSTYLE);
  if (h->opt_current == NBD_OPT_STARTTLS)
    assert (h->opt_mode);
  else {
    /* If TLS was not requested we skip this option and go to the next one. */
    if (h->tls == LIBNBD_TLS_DISABLE) {
      SET_NEXT_STATE (%^OPT_STRUCTURED_REPLY.START);
      return 0;
    }
    assert (CALLBACK_IS_NULL (h->opt_cb.completion));
  }

  h->sbuf.option.version = htobe64 (NBD_NEW_VERSION);
  h->sbuf.option.option = htobe32 (NBD_OPT_STARTTLS);
  h->sbuf.option.optlen = 0;
  h->chunks_sent++;
  h->wbuf = &h->sbuf;
  h->wlen = sizeof (h->sbuf.option);
  SET_NEXT_STATE (%SEND);
  return 0;

 NEWSTYLE.OPT_STARTTLS.SEND:
  switch (send_from_wbuf (h)) {
  case -1: SET_NEXT_STATE (%.DEAD); return 0;
  case 0:
    h->rbuf = &h->sbuf;
    h->rlen = sizeof (h->sbuf.or.option_reply);
    SET_NEXT_STATE (%RECV_REPLY);
  }
  return 0;

 NEWSTYLE.OPT_STARTTLS.RECV_REPLY:
  switch (recv_into_rbuf (h)) {
  case -1: SET_NEXT_STATE (%.DEAD); return 0;
  case 0:
    if (prepare_for_reply_payload (h, NBD_OPT_STARTTLS) == -1) {
      SET_NEXT_STATE (%.DEAD);
      return 0;
    }
    SET_NEXT_STATE (%RECV_REPLY_PAYLOAD);
  }
  return 0;

 NEWSTYLE.OPT_STARTTLS.RECV_REPLY_PAYLOAD:
  switch (recv_into_rbuf (h)) {
  case -1: SET_NEXT_STATE (%.DEAD); return 0;
  case 0:  SET_NEXT_STATE (%CHECK_REPLY);
  }
  return 0;

 NEWSTYLE.OPT_STARTTLS.CHECK_REPLY:
  uint32_t reply;
  struct socket *new_sock;
  int err = ENOTSUP;

  reply = be32toh (h->sbuf.or.option_reply.reply);
  switch (reply) {
  case NBD_REP_ACK:
    if (h->tls_negotiated) {
      set_error (EPROTO,
                 "handshake: unable to support server accepting TLS twice");
      SET_NEXT_STATE (%.DEAD);
      return 0;
    }
    nbd_internal_reset_size_and_flags (h);
    h->structured_replies = false;
    h->meta_valid = false;
    new_sock = nbd_internal_crypto_create_session (h, h->sock);
    if (new_sock == NULL) {
      SET_NEXT_STATE (%.DEAD);
      return 0;
    }
    h->sock = new_sock;
    if (nbd_internal_crypto_is_reading (h))
      SET_NEXT_STATE (%TLS_HANDSHAKE_READ);
    else
      SET_NEXT_STATE (%TLS_HANDSHAKE_WRITE);
    return 0;

  case NBD_REP_ERR_INVALID:
    err = EINVAL;
    /* fallthrough */
  default:
    if (handle_reply_error (h) == -1) {
      SET_NEXT_STATE (%.DEAD);
      return 0;
    }

    /* Server refused to upgrade to TLS.  If h->tls is not 'require' (2)
     * then we can continue unencrypted.
     */
    if (h->tls == LIBNBD_TLS_REQUIRE) {
      SET_NEXT_STATE (%^PREPARE_OPT_ABORT);
      set_error (ENOTSUP, "handshake: server refused TLS, "
                 "but handle TLS setting is 'require' (2)");
      return 0;
    }

    debug (h, "server refused TLS (%s)",
           reply == NBD_REP_ERR_POLICY ? "policy" :
           reply == NBD_REP_ERR_INVALID ? "invalid request" : "not supported");
    CALL_CALLBACK (h->opt_cb.completion, &err);
    nbd_internal_free_option (h);
    if (h->opt_current == NBD_OPT_STARTTLS)
      SET_NEXT_STATE (%.NEGOTIATING);
    else {
      debug (h, "continuing with unencrypted connection");
      SET_NEXT_STATE (%^OPT_STRUCTURED_REPLY.START);
    }
    return 0;
  }
  return 0;

 NEWSTYLE.OPT_STARTTLS.TLS_HANDSHAKE_READ:
  int r;

  r = nbd_internal_crypto_handshake (h);
  if (r == -1) {
    SET_NEXT_STATE (%.DEAD);
    return 0;
  }
  if (r == 0) {
    SET_NEXT_STATE (%TLS_HANDSHAKE_DONE);
    return 0;
  }
  /* Continue handshake. */
  if (nbd_internal_crypto_is_reading (h))
    SET_NEXT_STATE (%TLS_HANDSHAKE_READ);
  else
    SET_NEXT_STATE (%TLS_HANDSHAKE_WRITE);
  return 0;

 NEWSTYLE.OPT_STARTTLS.TLS_HANDSHAKE_WRITE:
  int r;

  r = nbd_internal_crypto_handshake (h);
  if (r == -1) {
    SET_NEXT_STATE (%.DEAD);
    return 0;
  }
  if (r == 0) {
    SET_NEXT_STATE (%TLS_HANDSHAKE_DONE);
    return 0;
  }
  /* Continue handshake. */
  if (nbd_internal_crypto_is_reading (h))
    SET_NEXT_STATE (%TLS_HANDSHAKE_READ);
  else
    SET_NEXT_STATE (%TLS_HANDSHAKE_WRITE);
  return 0;

 NEWSTYLE.OPT_STARTTLS.TLS_HANDSHAKE_DONE:
  int err = 0;

  /* Finished handshake. */
  h->tls_negotiated = true;
  nbd_internal_crypto_debug_tls_enabled (h);
  CALL_CALLBACK (h->opt_cb.completion, &err);
  nbd_internal_free_option (h);

  /* Continue with option negotiation. */
  if (h->opt_current == NBD_OPT_STARTTLS)
    SET_NEXT_STATE (%.NEGOTIATING);
  else
    SET_NEXT_STATE (%^OPT_STRUCTURED_REPLY.START);
  return 0;

} /* END STATE MACHINE */
