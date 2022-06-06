# libnbd Python bindings
# Copyright (C) 2010-2022 Red Hat Inc.
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
import sys
from array import array

h = nbd.NBD()
h.connect_command(["nbdkit", "-s", "--exit-with-parent", "-v",
                   "pattern", "size=1024"])
buf = nbd.Buffer(512)
cookie = h.aio_pread(buf, 0)
while not h.aio_command_completed(cookie):
    h.poll(-1)

buf1 = buf.to_bytearray()

# Prove that .to_bytearray() defaults to copying, even if buf is reused

cookie = h.aio_pread(buf, 512)
while not h.aio_command_completed(cookie):
    h.poll(-1)

buf2 = buf.to_bytearray()

print("%r" % buf1)

# The nbdkit pattern plugin exposes 64-bit numbers in bigendian order
arr = array('q', range(0, 1024, 8))
if sys.byteorder == 'little':
    arr.byteswap()
assert buf1 == memoryview(arr).cast('B')[:512]
assert buf2 == memoryview(arr).cast('B')[512:]
