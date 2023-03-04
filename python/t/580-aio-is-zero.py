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

# This tests the nbd.Buffer is_zero method.  We can do this entirely
# without using the NBD protocol.

import nbd

# Simplest case: A Buffer initialized with zeros should be zero.
ba = bytearray(2**20)
buf = nbd.Buffer.from_bytearray(ba)
assert buf.size() == 2**20
assert len(buf) == 2**20
assert buf.is_zero() is True

# The above buffer is 2**20 (= 1MB), slices of it should also be zero.
for i in range(0, 7):
    assert buf.is_zero(i * 2**17, 2**17) is True

# Alternative initializations
buf = nbd.Buffer.from_bytearray(1024)
assert buf.size() == 1024
assert buf.is_zero() is True

buf = nbd.Buffer.from_bytearray(b"\0" * 1024)
assert buf.size() == 1024
assert buf.is_zero() is True

# A Buffer initialized with non-zeroes should not be zero.
ba = bytearray(b'\xff') * 2**20
buf = nbd.Buffer.from_bytearray(ba)
assert buf.is_zero() is False

# Slices should not be zero.
for i in range(0, 15):
    assert buf.is_zero(i * 2**16, 2**16) is False

# Alternative initializations
buf = nbd.Buffer.from_bytearray(b"Hello world")
assert buf.size() == len(b"Hello world")
assert buf.is_zero() is False

buf = nbd.Buffer.from_bytearray([0x31, 0x32, 0x33])
assert buf.to_bytearray() == b"123"
assert buf.is_zero() is False

# Buffer with a block of non-zeroes and block of zeroes.
ba = bytearray(b'\xff') * 2**20 + bytearray(2**20)
buf = nbd.Buffer.from_bytearray(ba)
assert buf.is_zero() is False
assert buf.is_zero(0, 2**20) is False
assert buf.is_zero(2**20) is True
assert buf.is_zero(2**19) is False
assert buf.is_zero(2**20, 2**19) is True
assert buf.is_zero(2**20-1, 1) is False
assert buf.is_zero(2**20, 1) is True
assert buf.is_zero(0, 1) is False
assert buf.is_zero(2**21-1, 1) is True

# A buffer created with only a size is generally uninitialized until
# used with aio_pread; but it will be zeroed if accessed prematurely
buf = nbd.Buffer(1024)
assert buf.size() == 1024
assert len(buf) == 1024
assert buf.is_zero() is True
assert nbd.Buffer.from_bytearray(buf.to_bytearray()).is_zero()
