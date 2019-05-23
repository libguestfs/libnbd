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

/* State machine for ending newstyle handshake with NBD_OPT_EXPORT_NAME. */

/* STATE MACHINE */ {
 NEWSTYLE.OPT_EXPORT_NAME.START:
  conn->sbuf.option.version = htobe64 (NBD_NEW_VERSION);
  conn->sbuf.option.option = htobe32 (NBD_OPT_EXPORT_NAME);
  conn->sbuf.option.optlen = strlen (h->export_name);
  conn->wbuf = &conn->sbuf;
  conn->wlen = sizeof conn->sbuf.option;
  SET_NEXT_STATE (%SEND);
  return 0;

 NEWSTYLE.OPT_EXPORT_NAME.SEND:
  switch (send_from_wbuf (conn)) {
  case -1: SET_NEXT_STATE (%.DEAD); return -1;
  case 0:
    conn->wbuf = h->export_name;
    conn->wlen = strlen (h->export_name);
    SET_NEXT_STATE (%SEND_EXPORT);
  }
  return 0;

 NEWSTYLE.OPT_EXPORT_NAME.SEND_EXPORT:
  switch (send_from_wbuf (conn)) {
  case -1: SET_NEXT_STATE (%.DEAD); return -1;
  case 0:
    conn->rbuf = &conn->sbuf;
    conn->rlen = sizeof conn->sbuf.export_name_reply;
    if ((conn->gflags & NBD_FLAG_NO_ZEROES) != 0)
      conn->rlen -= sizeof conn->sbuf.export_name_reply.zeroes;
    SET_NEXT_STATE (%RECV_REPLY);
  }
  return 0;

 NEWSTYLE.OPT_EXPORT_NAME.RECV_REPLY:
  switch (recv_into_rbuf (conn)) {
  case -1: SET_NEXT_STATE (%.DEAD); return -1;
  case 0:  SET_NEXT_STATE (%CHECK_REPLY);
  }
  return 0;

 NEWSTYLE.OPT_EXPORT_NAME.CHECK_REPLY:
  uint64_t exportsize;
  uint16_t eflags;

  exportsize = be64toh (conn->sbuf.export_name_reply.exportsize);
  eflags = be16toh (conn->sbuf.export_name_reply.eflags);
  if (nbd_internal_set_size_and_flags (conn->h, exportsize, eflags) == -1) {
    SET_NEXT_STATE (%.DEAD);
    return -1;
  }
  SET_NEXT_STATE (%.READY);
  return 0;

} /* END STATE MACHINE */
