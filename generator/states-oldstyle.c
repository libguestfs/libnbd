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

STATE_MACHINE {
 OLDSTYLE.START:
  /* We've already read the first 16 bytes of the handshake, we must
   * now read the remainder.
   */
  h->rbuf = &h->sbuf.old_handshake;
  h->rlen = sizeof h->sbuf.old_handshake;
  h->rbuf += 16;
  h->rlen -= 16;
  SET_NEXT_STATE (%RECV_REMAINING);
  return 0;

 OLDSTYLE.RECV_REMAINING:
  switch (recv_into_rbuf (h)) {
  case -1: SET_NEXT_STATE (%.DEAD); return 0;
  case 0:  SET_NEXT_STATE (%CHECK);
  }
  return 0;

 OLDSTYLE.CHECK:
  uint64_t exportsize;
  uint16_t gflags, eflags;

  /* We already checked the magic and version in MAGIC.CHECK_MAGIC. */
  exportsize = be64toh (h->sbuf.old_handshake.exportsize);
  gflags = be16toh (h->sbuf.old_handshake.gflags);
  eflags = be16toh (h->sbuf.old_handshake.eflags);

  /* Server is unable to upgrade to TLS.  If h->tls is not 'require' (2)
   * then we can continue unencrypted.
   */
  if (h->tls == LIBNBD_TLS_REQUIRE) {
    SET_NEXT_STATE (%.DEAD);
    set_error (ENOTSUP, "handshake: server is oldstyle, "
               "but handle TLS setting is 'require' (2)");
    return 0;
  }

  h->gflags = gflags;
  debug (h, "gflags: 0x%" PRIx16, gflags);
  if (gflags) {
    set_error (0, "handshake: oldstyle server should not set gflags");
    SET_NEXT_STATE (%.DEAD);
    return 0;
  }

  if (nbd_internal_set_size_and_flags (h, exportsize, eflags) == -1) {
    SET_NEXT_STATE (%.DEAD);
    return 0;
  }

  h->protocol = "oldstyle";

  SET_NEXT_STATE (%.READY);

  return 0;

} /* END STATE MACHINE */
