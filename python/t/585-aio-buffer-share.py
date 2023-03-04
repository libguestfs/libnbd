# libnbd Python bindings
# Copyright Red Hat
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

# This tests various nbd.Buffer methods.  We can do this entirely
# without using the NBD protocol.

import nbd

# Use of to/from_bytearray always creates copies
ba = bytearray(512)
buf = nbd.Buffer.from_bytearray(ba)
ba.append(1)
assert len(ba) == 513
assert len(buf) == 512
assert buf.is_zero() is True
assert buf.to_bytearray() is not ba

# Use of to/from_buffer shares the same buffer
buf = nbd.Buffer.from_buffer(ba)
assert buf.is_zero() is False
assert len(buf) == 513
ba.pop()
assert buf.is_zero() is True
assert len(buf) == 512
assert buf.to_buffer() is ba

# Even though nbd.Buffer(n) start uninitialized, we sanitize before exporting.
# This test cheats and examines the private member ._init
buf = nbd.Buffer(512)
assert buf.is_zero() is True
assert hasattr(buf, '_init') is False
assert buf.to_buffer() == bytearray(512)
assert hasattr(buf, '_init') is True
