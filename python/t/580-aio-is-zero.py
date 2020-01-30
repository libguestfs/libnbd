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

# This tests the nbd.Buffer is_zero method.  We can do this entirely
# without using the NBD protocol.

import os
import nbd

# Simplest case: A Buffer initialized with zeros should be zero.
ba = bytearray (2**20)
buf = nbd.Buffer.from_bytearray (ba)
assert buf.is_zero ()

# The above buffer is 2**20 (= 1MB), slices of it should also be zero.
for i in range (0, 7):
    assert buf.is_zero (i * 2**17, 2**17)

# A Buffer initialized with non-zeroes should not be zero.
ba = bytearray (b'\xff') * 2**20
buf = nbd.Buffer.from_bytearray (ba)
assert not buf.is_zero ()

# Slices should not be zero.
for i in range (0, 15):
    assert not buf.is_zero (i * 2**16, 2**16)

# Buffer with a block of non-zeroes and block of zeroes.
ba = bytearray (b'\xff') * 2**20 + bytearray (2**20)
buf = nbd.Buffer.from_bytearray (ba)
assert not buf.is_zero ()
assert not buf.is_zero (0, 2**20)
assert buf.is_zero (2**20)
assert not buf.is_zero (2**19)
assert buf.is_zero (2**20, 2**19)
assert not buf.is_zero (2**20-1, 1)
assert buf.is_zero (2**20, 1)
assert not buf.is_zero (0, 1)
assert buf.is_zero (2**21-1, 1)
