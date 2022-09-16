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

/* State machine for negotiating NBD_OPT_SET/LIST_META_CONTEXT. */

STATE_MACHINE {
 NEWSTYLE.OPT_META_CONTEXT.START:
  size_t i;
  uint32_t len, opt;

  /* This state group is reached from:
   * h->opt_mode == false (h->opt_current == 0):
   *   nbd_connect_*()
   *     -> conditionally use SET, next state OPT_GO for NBD_OPT_GO
   * h->opt_mode == true (h->opt_current matches calling API):
   *   nbd_opt_info()
   *     -> conditionally use SET, next state OPT_GO for NBD_OPT_INFO
   *   nbd_opt_go()
   *     -> conditionally use SET, next state OPT_GO for NBD_OPT_GO
   *   nbd_opt_list_meta_context()
   *     -> unconditionally use LIST, next state NEGOTIATING
   *
   * For now, we start by unconditionally clearing h->exportsize and friends,
   * as well as h->meta_contexts and h->meta_valid.
   * If SET is conditional, we skip it if structured replies were
   * not negotiated, or if there were no contexts to request.
   * SET then manipulates h->meta_contexts, and sets h->meta_valid on success.
   * If OPT_GO is later successful, it populates h->exportsize and friends,
   * and also sets h->meta_valid if we skipped SET here.
   * There is a callback if and only if the command is LIST.
   */
  assert (h->gflags & LIBNBD_HANDSHAKE_FLAG_FIXED_NEWSTYLE);
  nbd_internal_reset_size_and_flags (h);
  for (i = 0; i < h->meta_contexts.len; ++i)
    free (h->meta_contexts.ptr[i].name);
  meta_vector_reset (&h->meta_contexts);
  if (h->opt_current == NBD_OPT_LIST_META_CONTEXT) {
    assert (h->opt_mode);
    assert (CALLBACK_IS_NOT_NULL (h->opt_cb.fn.context));
    opt = h->opt_current;
  }
  else {
    assert (CALLBACK_IS_NULL (h->opt_cb.fn.context));
    opt = NBD_OPT_SET_META_CONTEXT;
    if (!h->structured_replies || h->request_meta_contexts.len == 0) {
      SET_NEXT_STATE (%^OPT_GO.START);
      return 0;
    }
    if (nbd_internal_set_querylist (h, NULL) == -1) {
      SET_NEXT_STATE (%.DEAD);
      return 0;
    }
  }

  assert (!h->meta_valid);

  /* Calculate the length of the option request data. */
  len = 4 /* exportname len */ + strlen (h->export_name) + 4 /* nr queries */;
  for (i = 0; i < h->querylist.len; ++i)
    len += 4 /* length of query */ + strlen (h->querylist.ptr[i]);

  h->sbuf.option.version = htobe64 (NBD_NEW_VERSION);
  h->sbuf.option.option = htobe32 (opt);
  h->sbuf.option.optlen = htobe32 (len);
  h->chunks_sent++;
  h->wbuf = &h->sbuf;
  h->wlen = sizeof (h->sbuf.option);
  h->wflags = MSG_MORE;
  SET_NEXT_STATE (%SEND);
  return 0;

 NEWSTYLE.OPT_META_CONTEXT.SEND:
  switch (send_from_wbuf (h)) {
  case -1: SET_NEXT_STATE (%.DEAD); return 0;
  case 0:
    h->sbuf.len = htobe32 (strlen (h->export_name));
    h->wbuf = &h->sbuf.len;
    h->wlen = sizeof h->sbuf.len;
    h->wflags = MSG_MORE;
    SET_NEXT_STATE (%SEND_EXPORTNAMELEN);
  }
  return 0;

 NEWSTYLE.OPT_META_CONTEXT.SEND_EXPORTNAMELEN:
  switch (send_from_wbuf (h)) {
  case -1: SET_NEXT_STATE (%.DEAD); return 0;
  case 0:
    h->wbuf = h->export_name;
    h->wlen = strlen (h->export_name);
    h->wflags = MSG_MORE;
    SET_NEXT_STATE (%SEND_EXPORTNAME);
  }
  return 0;

 NEWSTYLE.OPT_META_CONTEXT.SEND_EXPORTNAME:
  switch (send_from_wbuf (h)) {
  case -1: SET_NEXT_STATE (%.DEAD); return 0;
  case 0:
    h->sbuf.nrqueries = htobe32 (h->querylist.len);
    h->wbuf = &h->sbuf;
    h->wlen = sizeof h->sbuf.nrqueries;
    h->wflags = MSG_MORE;
    SET_NEXT_STATE (%SEND_NRQUERIES);
  }
  return 0;

 NEWSTYLE.OPT_META_CONTEXT.SEND_NRQUERIES:
  switch (send_from_wbuf (h)) {
  case -1: SET_NEXT_STATE (%.DEAD); return 0;
  case 0:
    h->querynum = 0;
    SET_NEXT_STATE (%PREPARE_NEXT_QUERY);
  }
  return 0;

 NEWSTYLE.OPT_META_CONTEXT.PREPARE_NEXT_QUERY:
  if (h->querynum >= h->querylist.len) {
    /* end of list of requested meta contexts */
    SET_NEXT_STATE (%PREPARE_FOR_REPLY);
    return 0;
  }
  const char *query = h->querylist.ptr[h->querynum];

  h->sbuf.len = htobe32 (strlen (query));
  h->wbuf = &h->sbuf.len;
  h->wlen = sizeof h->sbuf.len;
  h->wflags = MSG_MORE;
  SET_NEXT_STATE (%SEND_QUERYLEN);
  return 0;

 NEWSTYLE.OPT_META_CONTEXT.SEND_QUERYLEN:
  const char *query = h->querylist.ptr[h->querynum];

  switch (send_from_wbuf (h)) {
  case -1: SET_NEXT_STATE (%.DEAD); return 0;
  case 0:
    h->wbuf = query;
    h->wlen = strlen (query);
    SET_NEXT_STATE (%SEND_QUERY);
  }
  return 0;

 NEWSTYLE.OPT_META_CONTEXT.SEND_QUERY:
  switch (send_from_wbuf (h)) {
  case -1: SET_NEXT_STATE (%.DEAD); return 0;
  case 0:
    h->querynum++;
    SET_NEXT_STATE (%PREPARE_NEXT_QUERY);
  }
  return 0;

 NEWSTYLE.OPT_META_CONTEXT.PREPARE_FOR_REPLY:
  h->rbuf = &h->sbuf.or.option_reply;
  h->rlen = sizeof h->sbuf.or.option_reply;
  SET_NEXT_STATE (%RECV_REPLY);
  return 0;

 NEWSTYLE.OPT_META_CONTEXT.RECV_REPLY:
  uint32_t opt;

  if (h->opt_current == NBD_OPT_LIST_META_CONTEXT)
    opt = h->opt_current;
  else
    opt = NBD_OPT_SET_META_CONTEXT;

  switch (recv_into_rbuf (h)) {
  case -1: SET_NEXT_STATE (%.DEAD); return 0;
  case 0:
    if (prepare_for_reply_payload (h, opt) == -1) {
      SET_NEXT_STATE (%.DEAD);
      return 0;
    }
    SET_NEXT_STATE (%RECV_REPLY_PAYLOAD);
  }
  return 0;

 NEWSTYLE.OPT_META_CONTEXT.RECV_REPLY_PAYLOAD:
  switch (recv_into_rbuf (h)) {
  case -1: SET_NEXT_STATE (%.DEAD); return 0;
  case 0:  SET_NEXT_STATE (%CHECK_REPLY);
  }
  return 0;

 NEWSTYLE.OPT_META_CONTEXT.CHECK_REPLY:
  uint32_t reply;
  uint32_t len;
  const size_t maxpayload = sizeof h->sbuf.or.payload.context;
  struct meta_context meta_context;
  uint32_t opt;
  int err = 0;

  if (h->opt_current == NBD_OPT_LIST_META_CONTEXT)
    opt = h->opt_current;
  else
    opt = NBD_OPT_SET_META_CONTEXT;

  reply = be32toh (h->sbuf.or.option_reply.reply);
  len = be32toh (h->sbuf.or.option_reply.replylen);
  switch (reply) {
  case NBD_REP_ACK:           /* End of list of replies. */
    if (opt == NBD_OPT_LIST_META_CONTEXT) {
      SET_NEXT_STATE (%.NEGOTIATING);
      CALL_CALLBACK (h->opt_cb.completion, &err);
      nbd_internal_free_option (h);
    }
    else {
      SET_NEXT_STATE (%^OPT_GO.START);
      h->meta_valid = true;
    }
    break;
  case NBD_REP_META_CONTEXT:  /* A context. */
    if (len > maxpayload)
      debug (h, "skipping too large meta context");
    else {
      assert (len > sizeof h->sbuf.or.payload.context.context.context_id);
      meta_context.context_id =
        be32toh (h->sbuf.or.payload.context.context.context_id);
      /* String payload is not NUL-terminated. */
      meta_context.name = strndup (h->sbuf.or.payload.context.str,
                                   len - sizeof meta_context.context_id);
      if (meta_context.name == NULL) {
        set_error (errno, "strdup");
        SET_NEXT_STATE (%.DEAD);
        return 0;
      }
      debug (h, "negotiated %s with context ID %" PRIu32,
             meta_context.name, meta_context.context_id);
      if (opt == NBD_OPT_LIST_META_CONTEXT) {
        CALL_CALLBACK (h->opt_cb.fn.context, meta_context.name);
        free (meta_context.name);
      }
      else if (meta_vector_append (&h->meta_contexts, meta_context) == -1) {
        set_error (errno, "realloc");
        free (meta_context.name);
        SET_NEXT_STATE (%.DEAD);
        return 0;
      }
    }
    SET_NEXT_STATE (%PREPARE_FOR_REPLY);
    break;
  default:
    /* Anything else is an error, ignore it for SET, report it for LIST */
    if (handle_reply_error (h) == -1) {
      SET_NEXT_STATE (%.DEAD);
      return 0;
    }

    if (opt == NBD_OPT_LIST_META_CONTEXT) {
      /* XXX Should we decode specific expected errors, like
       * REP_ERR_UNKNOWN to ENOENT or REP_ERR_TOO_BIG to ERANGE?
       */
      err = ENOTSUP;
      set_error (err, "unexpected response, possibly the server does not "
                 "support listing contexts");
      CALL_CALLBACK (h->opt_cb.completion, &err);
      nbd_internal_free_option (h);
      SET_NEXT_STATE (%.NEGOTIATING);
    }
    else {
      debug (h, "handshake: unexpected error from "
             "NBD_OPT_SET_META_CONTEXT (%" PRIu32 ")", reply);
      SET_NEXT_STATE (%^OPT_GO.START);
    }
    break;
  }
  return 0;

} /* END STATE MACHINE */
