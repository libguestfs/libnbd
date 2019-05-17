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

/* State machine for parsing the initial magic number from the server. */

/* STATE MACHINE */ {
 MAGIC.START:
  conn->rbuf = &conn->sbuf;
  conn->rlen = 16;
  SET_NEXT_STATE (%RECV_MAGIC);
  return 0;

 MAGIC.RECV_MAGIC:
  switch (recv_into_rbuf (conn)) {
  case -1: SET_NEXT_STATE (%.DEAD); return -1;
  case 0:  SET_NEXT_STATE (%CHECK_MAGIC);
  }
  return 0;

 MAGIC.CHECK_MAGIC:
  if (strncmp (conn->sbuf.handshake.nbdmagic, "NBDMAGIC", 8) != 0) {
    SET_NEXT_STATE (%.DEAD);
    set_error (0, "handshake: server did not send expected NBD magic");
    return -1;
  }
  /* XXX Only handle fixed newstyle servers for now. */
  conn->sbuf.handshake.version = be64toh (conn->sbuf.handshake.version);
  if (conn->sbuf.handshake.version != NBD_NEW_VERSION) {
    SET_NEXT_STATE (%.DEAD);
    set_error (0, "handshake: server is not a fixed newstyle NBD server");
    return -1;
  }
  SET_NEXT_STATE (%.NEWSTYLE.START);
  return 0;

} /* END STATE MACHINE */
