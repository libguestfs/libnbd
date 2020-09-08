# libnbd Python bindings
# Copyright (C) 2010-2020 Red Hat Inc.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA

import nbd

h = nbd.NBD()
h.set_export_name("name")
assert h.get_export_name() == "name"
h.set_full_info(True)
assert h.get_full_info() is True
try:
    h.set_tls(nbd.TLS_REQUIRE + 1)
    assert False
except nbd.Error:
    pass
assert h.get_tls() == nbd.TLS_DISABLE
if h.supports_tls():
    h.set_tls(nbd.TLS_ALLOW)
    assert h.get_tls() == nbd.TLS_ALLOW
h.set_request_structured_replies(False)
assert h.get_request_structured_replies() is False
try:
    h.set_handshake_flags(nbd.HANDSHAKE_FLAG_NO_ZEROES << 1)
    assert False
except nbd.Error:
    assert h.get_handshake_flags() == (nbd.HANDSHAKE_FLAG_NO_ZEROES |
                                       nbd.HANDSHAKE_FLAG_FIXED_NEWSTYLE)
h.set_handshake_flags(0)
assert h.get_handshake_flags() == 0
h.set_opt_mode(True)
assert h.get_opt_mode() is True
