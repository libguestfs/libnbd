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

/* State machine for negotiating NBD_OPT_SET_META_CONTEXT. */

STATE_MACHINE {
 NEWSTYLE.OPT_SET_META_CONTEXT.START:
  size_t i, nr_queries;
  uint32_t len;

  /* If the server doesn't support SRs then we must skip this group.
   * Also we skip the group if the client didn't request any metadata
   * contexts.
   */
  if (!h->structured_replies ||
      h->request_meta_contexts == NULL ||
      nbd_internal_string_list_length (h->request_meta_contexts) == 0) {
    SET_NEXT_STATE (%^OPT_GO.START);
    return 0;
  }

  assert (h->meta_contexts == NULL);

  /* Calculate the length of the option request data. */
  len = 4 /* exportname len */ + strlen (h->export_name) + 4 /* nr queries */;
  nr_queries = nbd_internal_string_list_length (h->request_meta_contexts);
  for (i = 0; i < nr_queries; ++i)
    len += 4 /* length of query */ + strlen (h->request_meta_contexts[i]);

  h->sbuf.option.version = htobe64 (NBD_NEW_VERSION);
  h->sbuf.option.option = htobe32 (NBD_OPT_SET_META_CONTEXT);
  h->sbuf.option.optlen = htobe32 (len);
  h->wbuf = &h->sbuf;
  h->wlen = sizeof (h->sbuf.option);
  h->wflags = MSG_MORE;
  SET_NEXT_STATE (%SEND);
  return 0;

 NEWSTYLE.OPT_SET_META_CONTEXT.SEND:
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

 NEWSTYLE.OPT_SET_META_CONTEXT.SEND_EXPORTNAMELEN:
  switch (send_from_wbuf (h)) {
  case -1: SET_NEXT_STATE (%.DEAD); return 0;
  case 0:
    h->wbuf = h->export_name;
    h->wlen = strlen (h->export_name);
    h->wflags = MSG_MORE;
    SET_NEXT_STATE (%SEND_EXPORTNAME);
  }
  return 0;

 NEWSTYLE.OPT_SET_META_CONTEXT.SEND_EXPORTNAME:
  switch (send_from_wbuf (h)) {
  case -1: SET_NEXT_STATE (%.DEAD); return 0;
  case 0:
    h->sbuf.nrqueries =
      htobe32 (nbd_internal_string_list_length (h->request_meta_contexts));
    h->wbuf = &h->sbuf;
    h->wlen = sizeof h->sbuf.nrqueries;
    h->wflags = MSG_MORE;
    SET_NEXT_STATE (%SEND_NRQUERIES);
  }
  return 0;

 NEWSTYLE.OPT_SET_META_CONTEXT.SEND_NRQUERIES:
  switch (send_from_wbuf (h)) {
  case -1: SET_NEXT_STATE (%.DEAD); return 0;
  case 0:
    h->querynum = 0;
    SET_NEXT_STATE (%PREPARE_NEXT_QUERY);
  }
  return 0;

 NEWSTYLE.OPT_SET_META_CONTEXT.PREPARE_NEXT_QUERY:
  const char *query = h->request_meta_contexts[h->querynum];

  if (query == NULL) { /* end of list of requested meta contexts */
    SET_NEXT_STATE (%PREPARE_FOR_REPLY);
    return 0;
  }

  h->sbuf.len = htobe32 (strlen (query));
  h->wbuf = &h->sbuf.len;
  h->wlen = sizeof h->sbuf.len;
  h->wflags = MSG_MORE;
  SET_NEXT_STATE (%SEND_QUERYLEN);
  return 0;

 NEWSTYLE.OPT_SET_META_CONTEXT.SEND_QUERYLEN:
  const char *query = h->request_meta_contexts[h->querynum];

  switch (send_from_wbuf (h)) {
  case -1: SET_NEXT_STATE (%.DEAD); return 0;
  case 0:
    h->wbuf = query;
    h->wlen = strlen (query);
    SET_NEXT_STATE (%SEND_QUERY);
  }
  return 0;

 NEWSTYLE.OPT_SET_META_CONTEXT.SEND_QUERY:
  switch (send_from_wbuf (h)) {
  case -1: SET_NEXT_STATE (%.DEAD); return 0;
  case 0:
    h->querynum++;
    SET_NEXT_STATE (%PREPARE_NEXT_QUERY);
  }
  return 0;

 NEWSTYLE.OPT_SET_META_CONTEXT.PREPARE_FOR_REPLY:
  h->rbuf = &h->sbuf.or.option_reply;
  h->rlen = sizeof h->sbuf.or.option_reply;
  SET_NEXT_STATE (%RECV_REPLY);
  return 0;

 NEWSTYLE.OPT_SET_META_CONTEXT.RECV_REPLY:
  switch (recv_into_rbuf (h)) {
  case -1: SET_NEXT_STATE (%.DEAD); return 0;
  case 0:
    if (prepare_for_reply_payload (h, NBD_OPT_SET_META_CONTEXT) == -1) {
      SET_NEXT_STATE (%.DEAD);
      return 0;
    }
    SET_NEXT_STATE (%RECV_REPLY_PAYLOAD);
  }
  return 0;

 NEWSTYLE.OPT_SET_META_CONTEXT.RECV_REPLY_PAYLOAD:
  switch (recv_into_rbuf (h)) {
  case -1: SET_NEXT_STATE (%.DEAD); return 0;
  case 0:  SET_NEXT_STATE (%CHECK_REPLY);
  }
  return 0;

 NEWSTYLE.OPT_SET_META_CONTEXT.CHECK_REPLY:
  uint32_t reply;
  uint32_t len;
  const size_t maxpayload = sizeof h->sbuf.or.payload.context;
  struct meta_context *meta_context;

  reply = be32toh (h->sbuf.or.option_reply.reply);
  len = be32toh (h->sbuf.or.option_reply.replylen);
  switch (reply) {
  case NBD_REP_ACK:           /* End of list of replies. */
    SET_NEXT_STATE (%^OPT_GO.START);
    break;
  case NBD_REP_META_CONTEXT:  /* A context. */
    if (len > maxpayload)
      debug (h, "skipping too large meta context");
    else {
      assert (len > sizeof h->sbuf.or.payload.context.context.context_id);
      meta_context = malloc (sizeof *meta_context);
      if (meta_context == NULL) {
        set_error (errno, "malloc");
        SET_NEXT_STATE (%.DEAD);
        return 0;
      }
      meta_context->context_id =
        be32toh (h->sbuf.or.payload.context.context.context_id);
      /* String payload is not NUL-terminated. */
      meta_context->name = strndup (h->sbuf.or.payload.context.str,
                                    len - sizeof meta_context->context_id);
      if (meta_context->name == NULL) {
        set_error (errno, "strdup");
        SET_NEXT_STATE (%.DEAD);
        free (meta_context);
        return 0;
      }
      debug (h, "negotiated %s with context ID %" PRIu32,
             meta_context->name, meta_context->context_id);
      meta_context->next = h->meta_contexts;
      h->meta_contexts = meta_context;
    }
    SET_NEXT_STATE (%PREPARE_FOR_REPLY);
    break;
  default:
    /* Anything else is an error, ignore it */
    if (handle_reply_error (h) == -1) {
      SET_NEXT_STATE (%.DEAD);
      return 0;
    }

    debug (h, "handshake: unexpected error from "
           "NBD_OPT_SET_META_CONTEXT (%" PRIu32 ")", reply);
    SET_NEXT_STATE (%^OPT_GO.START);
    break;
  }
  return 0;

} /* END STATE MACHINE */
