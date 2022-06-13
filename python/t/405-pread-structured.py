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
import errno
import sys
from array import array

h = nbd.NBD()
h.connect_command(["nbdkit", "-s", "--exit-with-parent", "-v",
                   "pattern", "size=512"])

# The nbdkit pattern plugin exposes 64-bit numbers in bigendian order
arr = array('q', range(0, 512, 8))
if sys.byteorder == 'little':
    arr.byteswap()
expected = memoryview(arr).cast('B')
stash = None


def f(user_data, buf2, offset, s, err):
    assert err.value == 0
    err.value = errno.EPROTO
    if user_data != 42:
        raise ValueError('unexpected user_data')
    assert buf2 == expected
    try:
        buf2[0] = 1
        assert False
    except TypeError:
        pass
    assert offset == 0
    assert s == nbd.READ_DATA
    global stash
    stash = buf2


buf = h.pread_structured(512, 0, lambda *args: f(42, *args))

print("%r" % buf)

assert buf == expected

# The callback can stash its slice; as long as that is live, we can't
# resize buf but can view changes in buf through the slice
try:
    buf.pop()
    assert False
except BufferError:
    pass
buf[0] ^= 1
assert buf == stash
stash = None
buf.pop()

# Tests of error handling
buf = h.pread_structured(512, 0, lambda *args: f(42, *args),
                         nbd.CMD_FLAG_DF)

print("%r" % buf)

assert buf == expected

try:
    buf = h.pread_structured(512, 0, lambda *args: f(43, *args),
                             nbd.CMD_FLAG_DF)
    assert False
except nbd.Error as ex:
    assert ex.errnum == errno.EPROTO
