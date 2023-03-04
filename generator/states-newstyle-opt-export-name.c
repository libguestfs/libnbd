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

/* State machine for ending newstyle handshake with NBD_OPT_EXPORT_NAME. */

STATE_MACHINE {
 NEWSTYLE.OPT_EXPORT_NAME.START:
  h->sbuf.option.version = htobe64 (NBD_NEW_VERSION);
  h->sbuf.option.option = htobe32 (NBD_OPT_EXPORT_NAME);
  h->sbuf.option.optlen = htobe32 (strlen (h->export_name));
  h->chunks_sent++;
  h->wbuf = &h->sbuf;
  h->wlen = sizeof h->sbuf.option;
  h->wflags = MSG_MORE;
  SET_NEXT_STATE (%SEND);
  return 0;

 NEWSTYLE.OPT_EXPORT_NAME.SEND:
  switch (send_from_wbuf (h)) {
  case -1: SET_NEXT_STATE (%.DEAD); return 0;
  case 0:
    h->wbuf = h->export_name;
    h->wlen = strlen (h->export_name);
    SET_NEXT_STATE (%SEND_EXPORT);
  }
  return 0;

 NEWSTYLE.OPT_EXPORT_NAME.SEND_EXPORT:
  switch (send_from_wbuf (h)) {
  case -1: SET_NEXT_STATE (%.DEAD); return 0;
  case 0:
    h->rbuf = &h->sbuf;
    h->rlen = sizeof h->sbuf.export_name_reply;
    if ((h->gflags & LIBNBD_HANDSHAKE_FLAG_NO_ZEROES) != 0)
      h->rlen -= sizeof h->sbuf.export_name_reply.zeroes;
    SET_NEXT_STATE (%RECV_REPLY);
  }
  return 0;

 NEWSTYLE.OPT_EXPORT_NAME.RECV_REPLY:
  switch (recv_into_rbuf (h)) {
  case -1: SET_NEXT_STATE (%.DEAD); return 0;
  case 0:  SET_NEXT_STATE (%CHECK_REPLY);
  }
  return 0;

 NEWSTYLE.OPT_EXPORT_NAME.CHECK_REPLY:
  uint64_t exportsize;
  uint16_t eflags;
  int err = 0;

  exportsize = be64toh (h->sbuf.export_name_reply.exportsize);
  eflags = be16toh (h->sbuf.export_name_reply.eflags);
  if (nbd_internal_set_size_and_flags (h, exportsize, eflags) == -1) {
    SET_NEXT_STATE (%.DEAD);
    return 0;
  }
  nbd_internal_set_payload (h);
  SET_NEXT_STATE (%^FINISHED);
  CALL_CALLBACK (h->opt_cb.completion, &err);
  nbd_internal_free_option (h);
  return 0;

} /* END STATE MACHINE */
