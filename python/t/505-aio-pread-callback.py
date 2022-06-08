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

def chunk(user_data, buf2, offset, s, err):
    print("in chunk, user_data %d" % user_data)
    assert err.value == 0
    err.value = errno.EPROTO
    if user_data != 42:
        raise ValueError('unexpected user_data')
    assert buf2 == expected
    assert offset == 0
    assert s == nbd.READ_DATA
    global stash
    stash = buf2


def callback(user_data, err):
    print("in callback, user_data %d,%d" % user_data)
    if user_data[0] == 42:
        assert err.value == 0
    else:
        assert err.value == errno.EPROTO
    err.value = errno.ENOMEM
    if user_data[1] != 42:
        raise ValueError('unexpected user_data')


# First try: succeed in both callbacks
buf = bytearray(512)
cookie = h.aio_pread_structured(buf, 0,
                                lambda *args: chunk(42, *args),
                                lambda *args: callback((42, 42), *args))
while not h.aio_command_completed(cookie):
    h.poll(-1)

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

# Second try: fail only during callback
buf = nbd.Buffer(512)
cookie = h.aio_pread_structured(buf, 0,
                                lambda *args: chunk(42, *args),
                                lambda *args: callback((42, 43), *args))
try:
    while not h.aio_command_completed(cookie):
        h.poll(-1)
    assert False
except nbd.Error as ex:
    assert ex.errnum == errno.ENOMEM

# Third try: fail during both
buf = nbd.Buffer(512)
cookie = h.aio_pread_structured(buf, 0,
                                lambda *args: chunk(43, *args),
                                lambda *args: callback((43, 43), *args))
try:
    while not h.aio_command_completed(cookie):
        h.poll(-1)
    assert False
except nbd.Error as ex:
    assert ex.errnum == errno.ENOMEM

# Fourth try: fail only during chunk
buf = nbd.Buffer(512)
cookie = h.aio_pread_structured(buf, 0,
                                lambda *args: chunk(43, *args),
                                lambda *args: callback((43, 42), *args))
try:
    while not h.aio_command_completed(cookie):
        h.poll(-1)
    assert False
except nbd.Error as ex:
    assert ex.errnum == errno.EPROTO
