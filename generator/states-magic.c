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

STATE_MACHINE {
 MAGIC.START:
  h->rbuf = &h->sbuf;
  h->rlen = 16;
  SET_NEXT_STATE (%RECV_MAGIC);
  return 0;

 MAGIC.RECV_MAGIC:
  switch (recv_into_rbuf (h)) {
  case -1: SET_NEXT_STATE (%.DEAD); return 0;
  case 0:  SET_NEXT_STATE (%CHECK_MAGIC);
  }
  return 0;

 MAGIC.CHECK_MAGIC:
  uint64_t version;

  if (be64toh (h->sbuf.new_handshake.nbdmagic) != NBD_MAGIC) {
    SET_NEXT_STATE (%.DEAD);
    set_error (0, "handshake: server did not send expected NBD magic");
    return 0;
  }

  version = be64toh (h->sbuf.new_handshake.version);
  if (version == NBD_NEW_VERSION)
    SET_NEXT_STATE (%.NEWSTYLE.START);
  else if (version == NBD_OLD_VERSION)
    SET_NEXT_STATE (%.OLDSTYLE.START);
  else {
    SET_NEXT_STATE (%.DEAD);
    set_error (0, "handshake: server is not either an oldstyle or fixed newstyle NBD server");
    return 0;
  }
  return 0;

} /* END STATE MACHINE */
