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

/* State machine for parsing the oldstyle handshake. */

/* STATE MACHINE */ {
 OLDSTYLE.START:
  /* We've already read the first 16 bytes of the handshake, we must
   * now read the remainder.
   */
  conn->rbuf = &conn->sbuf.old_handshake;
  conn->rlen = sizeof conn->sbuf.old_handshake;
  conn->rbuf += 16;
  conn->rlen -= 16;
  SET_NEXT_STATE (%RECV_REMAINING);
  return 0;

 OLDSTYLE.RECV_REMAINING:
  switch (recv_into_rbuf (conn)) {
  case -1: SET_NEXT_STATE (%.DEAD); return -1;
  case 0:  SET_NEXT_STATE (%CHECK);
  }
  return 0;

 OLDSTYLE.CHECK:
  uint64_t exportsize;
  uint16_t gflags, eflags;

  /* We already checked the magic and version in MAGIC.CHECK_MAGIC. */
  exportsize = be64toh (conn->sbuf.old_handshake.exportsize);
  gflags = be16toh (conn->sbuf.old_handshake.gflags);
  eflags = be16toh (conn->sbuf.old_handshake.eflags);

  conn->gflags = gflags;
  conn->h->exportsize = exportsize;
  conn->h->eflags = eflags;
  debug (conn->h, "exportsize: %" PRIu64 " eflags: 0x%" PRIx16
         " gflags: 0x%" PRIx16,
         exportsize, eflags, gflags);
  if (eflags == 0) {
    SET_NEXT_STATE (%.DEAD);
    set_error (EINVAL, "handshake: invalid eflags == 0 from server");
    return -1;
  }

  SET_NEXT_STATE (%.READY);

  return 0;

} /* END STATE MACHINE */
