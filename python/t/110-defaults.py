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
assert h.get_export_name() == ""
assert h.get_full_info() is False
assert h.get_tls() == nbd.TLS_DISABLE
assert h.get_request_structured_replies() is True
assert h.get_handshake_flags() == (nbd.HANDSHAKE_FLAG_FIXED_NEWSTYLE |
                                   nbd.HANDSHAKE_FLAG_NO_ZEROES)
assert h.get_opt_mode() is False
