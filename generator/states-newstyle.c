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

#include <assert.h>

#include "internal.h"

/* Common code for parsing a reply to NBD_OPT_*. */
static int
prepare_for_reply_payload (struct nbd_handle *h, uint32_t opt)
{
  const size_t maxpayload = sizeof h->sbuf.or.payload;
  uint64_t magic;
  uint32_t option;
  uint32_t reply;
  uint32_t len;

  magic = be64toh (h->sbuf.or.option_reply.magic);
  option = be32toh (h->sbuf.or.option_reply.option);
  reply = be32toh (h->sbuf.or.option_reply.reply);
  len = be32toh (h->sbuf.or.option_reply.replylen);
  if (magic != NBD_REP_MAGIC || option != opt) {
    set_error (0, "handshake: invalid option reply magic or option");
    return -1;
  }

  /* Validate lengths that the state machine depends on. */
  switch (reply) {
  case NBD_REP_ACK:
    if (len != 0) {
      set_error (0, "handshake: invalid NBD_REP_ACK option reply length");
      return -1;
    }
    break;
  case NBD_REP_INFO:
    /* Can't enforce an upper bound, thanks to unknown INFOs */
    if (len < sizeof h->sbuf.or.payload.export.info) {
      set_error (0, "handshake: NBD_REP_INFO reply length too small");
      return -1;
    }
    break;
  case NBD_REP_META_CONTEXT:
    if (len <= sizeof h->sbuf.or.payload.context.context ||
        len > sizeof h->sbuf.or.payload.context) {
      set_error (0, "handshake: invalid NBD_REP_META_CONTEXT reply length");
      return -1;
    }
    break;
  }

  /* Read the following payload if it is short enough to fit in the
   * static buffer.  If it's too long, skip it.
   */
  len = be32toh (h->sbuf.or.option_reply.replylen);
  if (len > MAX_REQUEST_SIZE) {
    set_error (0, "handshake: invalid option reply length");
    return -1;
  }
  else if (len <= maxpayload)
    h->rbuf = &h->sbuf.or.payload;
  else
    h->rbuf = NULL;
  h->rlen = len;
  return 0;
}

/* Check an unexpected server reply. If it is an error, log any
 * message from the server and return 0; otherwise, return -1.
 */
static int
handle_reply_error (struct nbd_handle *h)
{
  uint32_t len;
  uint32_t reply;

  len = be32toh (h->sbuf.or.option_reply.replylen);
  reply = be32toh (h->sbuf.or.option_reply.reply);
  if (!NBD_REP_IS_ERR (reply)) {
    set_error (0, "handshake: unexpected option reply type %d", reply);
    return -1;
  }

  assert (NBD_MAX_STRING < sizeof h->sbuf.or.payload);
  if (len > NBD_MAX_STRING) {
    set_error (0, "handshake: option error string too long");
    return -1;
  }

  if (len > 0)
    debug (h, "handshake: server error message: %.*s", (int) len,
           h->sbuf.or.payload.err_msg);

  return 0;
}

/* State machine for parsing the fixed newstyle handshake. */

STATE_MACHINE {
 NEWSTYLE.START:
  h->rbuf = &h->sbuf;
  h->rlen = sizeof h->sbuf.gflags;
  SET_NEXT_STATE (%RECV_GFLAGS);
  return 0;

 NEWSTYLE.RECV_GFLAGS:
  switch (recv_into_rbuf (h)) {
  case -1: SET_NEXT_STATE (%.DEAD); return 0;
  case 0:  SET_NEXT_STATE (%CHECK_GFLAGS);
  }
  return 0;

 NEWSTYLE.CHECK_GFLAGS:
  uint32_t cflags;

  h->gflags &= be16toh (h->sbuf.gflags);
  if ((h->gflags & LIBNBD_HANDSHAKE_FLAG_FIXED_NEWSTYLE) == 0 &&
      h->tls == LIBNBD_TLS_REQUIRE) {
    SET_NEXT_STATE (%.DEAD);
    set_error (ENOTSUP, "handshake: server is not using fixed newstyle, "
               "but handle TLS setting is 'require' (2)");
    return 0;
  }

  cflags = h->gflags;
  h->sbuf.cflags = htobe32 (cflags);
  h->wbuf = &h->sbuf;
  h->wlen = 4;
  SET_NEXT_STATE (%SEND_CFLAGS);
  return 0;

 NEWSTYLE.SEND_CFLAGS:
  switch (send_from_wbuf (h)) {
  case -1: SET_NEXT_STATE (%.DEAD); return 0;
  case 0:
    /* Start sending options. */
    if ((h->gflags & LIBNBD_HANDSHAKE_FLAG_FIXED_NEWSTYLE) == 0)
      SET_NEXT_STATE (%OPT_EXPORT_NAME.START);
    else
      SET_NEXT_STATE (%OPT_STARTTLS.START);
  }
  return 0;

 NEWSTYLE.FINISHED:
  if ((h->gflags & LIBNBD_HANDSHAKE_FLAG_FIXED_NEWSTYLE) == 0)
    h->protocol = "newstyle";
  else
    h->protocol = "newstyle-fixed";

  SET_NEXT_STATE (%.READY);
  return 0;

} /* END STATE MACHINE */
