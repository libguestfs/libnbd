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

/* State machine for parsing the fixed newstyle handshake. */

/* STATE MACHINE */ {
 NEWSTYLE.START:
  conn->rbuf = &conn->gflags;
  conn->rlen = 2;
  SET_NEXT_STATE (%RECV_GFLAGS);
  return 0;

 NEWSTYLE.RECV_GFLAGS:
  switch (recv_into_rbuf (conn)) {
  case -1: SET_NEXT_STATE (%.DEAD); return -1;
  case 0:  SET_NEXT_STATE (%CHECK_GFLAGS);
  }
  return 0;

 NEWSTYLE.CHECK_GFLAGS:
  uint32_t cflags;

  conn->gflags = be16toh (conn->gflags);
  if ((conn->gflags & NBD_FLAG_FIXED_NEWSTYLE) == 0 &&
      h->tls == 2) {
    SET_NEXT_STATE (%.DEAD);
    set_error (ENOTSUP, "handshake: server is not fixed newstyle, "
	       "but handle TLS setting is require (2)");
    return -1;
  }

  cflags = conn->gflags & (NBD_FLAG_FIXED_NEWSTYLE|NBD_FLAG_NO_ZEROES);
  conn->sbuf.cflags = htobe32 (cflags);
  conn->wbuf = &conn->sbuf;
  conn->wlen = 4;
  SET_NEXT_STATE (%SEND_CFLAGS);
  return 0;

 NEWSTYLE.SEND_CFLAGS:
  switch (send_from_wbuf (conn)) {
  case -1: SET_NEXT_STATE (%.DEAD); return -1;
  case 0:
    /* Start sending options. */
    if ((conn->gflags & NBD_FLAG_FIXED_NEWSTYLE) == 0)
      SET_NEXT_STATE (%OPT_EXPORT_NAME.START);
    else
      SET_NEXT_STATE (%OPT_STARTTLS.START);
  }
  return 0;

} /* END STATE MACHINE */
