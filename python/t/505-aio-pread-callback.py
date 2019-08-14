# libnbd Python bindings
# Copyright (C) 2010-2019 Red Hat Inc.
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

h = nbd.NBD ()
h.connect_command (["nbdkit", "-s", "--exit-with-parent", "-v",
                    "pattern", "size=512"])

expected = b'\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x08\x00\x00\x00\x00\x00\x00\x00\x10\x00\x00\x00\x00\x00\x00\x00\x18\x00\x00\x00\x00\x00\x00\x00 \x00\x00\x00\x00\x00\x00\x00(\x00\x00\x00\x00\x00\x00\x000\x00\x00\x00\x00\x00\x00\x008\x00\x00\x00\x00\x00\x00\x00@\x00\x00\x00\x00\x00\x00\x00H\x00\x00\x00\x00\x00\x00\x00P\x00\x00\x00\x00\x00\x00\x00X\x00\x00\x00\x00\x00\x00\x00`\x00\x00\x00\x00\x00\x00\x00h\x00\x00\x00\x00\x00\x00\x00p\x00\x00\x00\x00\x00\x00\x00x\x00\x00\x00\x00\x00\x00\x00\x80\x00\x00\x00\x00\x00\x00\x00\x88\x00\x00\x00\x00\x00\x00\x00\x90\x00\x00\x00\x00\x00\x00\x00\x98\x00\x00\x00\x00\x00\x00\x00\xa0\x00\x00\x00\x00\x00\x00\x00\xa8\x00\x00\x00\x00\x00\x00\x00\xb0\x00\x00\x00\x00\x00\x00\x00\xb8\x00\x00\x00\x00\x00\x00\x00\xc0\x00\x00\x00\x00\x00\x00\x00\xc8\x00\x00\x00\x00\x00\x00\x00\xd0\x00\x00\x00\x00\x00\x00\x00\xd8\x00\x00\x00\x00\x00\x00\x00\xe0\x00\x00\x00\x00\x00\x00\x00\xe8\x00\x00\x00\x00\x00\x00\x00\xf0\x00\x00\x00\x00\x00\x00\x00\xf8\x00\x00\x00\x00\x00\x00\x01\x00\x00\x00\x00\x00\x00\x00\x01\x08\x00\x00\x00\x00\x00\x00\x01\x10\x00\x00\x00\x00\x00\x00\x01\x18\x00\x00\x00\x00\x00\x00\x01 \x00\x00\x00\x00\x00\x00\x01(\x00\x00\x00\x00\x00\x00\x010\x00\x00\x00\x00\x00\x00\x018\x00\x00\x00\x00\x00\x00\x01@\x00\x00\x00\x00\x00\x00\x01H\x00\x00\x00\x00\x00\x00\x01P\x00\x00\x00\x00\x00\x00\x01X\x00\x00\x00\x00\x00\x00\x01`\x00\x00\x00\x00\x00\x00\x01h\x00\x00\x00\x00\x00\x00\x01p\x00\x00\x00\x00\x00\x00\x01x\x00\x00\x00\x00\x00\x00\x01\x80\x00\x00\x00\x00\x00\x00\x01\x88\x00\x00\x00\x00\x00\x00\x01\x90\x00\x00\x00\x00\x00\x00\x01\x98\x00\x00\x00\x00\x00\x00\x01\xa0\x00\x00\x00\x00\x00\x00\x01\xa8\x00\x00\x00\x00\x00\x00\x01\xb0\x00\x00\x00\x00\x00\x00\x01\xb8\x00\x00\x00\x00\x00\x00\x01\xc0\x00\x00\x00\x00\x00\x00\x01\xc8\x00\x00\x00\x00\x00\x00\x01\xd0\x00\x00\x00\x00\x00\x00\x01\xd8\x00\x00\x00\x00\x00\x00\x01\xe0\x00\x00\x00\x00\x00\x00\x01\xe8\x00\x00\x00\x00\x00\x00\x01\xf0\x00\x00\x00\x00\x00\x00\x01\xf8'

def chunk (user_data, buf2, offset, s, err):
    print ("in chunk, user_data %d" % user_data)
    assert err.value == 0
    err.value = errno.EPROTO
    if user_data != 42:
        raise ValueError('unexpected user_data')
    assert buf2 == expected
    assert offset == 0
    assert s == nbd.READ_DATA

def callback (user_data, err):
    print ("in callback, user_data %d,%d" % user_data)
    if user_data[0] == 42:
        assert err.value == 0
    else:
        assert err.value == errno.EPROTO
    err.value = errno.ENOMEM
    if user_data[1] != 42:
        raise ValueError('unexpected user_data')

# First try: succeed in both callbacks
buf = nbd.Buffer (512)
cookie = h.aio_pread_structured (buf, 0,
                                 lambda *args: chunk (42, *args),
                                 lambda *args: callback ((42, 42), *args))
while not (h.aio_command_completed (cookie)):
    h.poll (-1)

buf = buf.to_bytearray ()

print ("%r" % buf)

assert buf == expected

# Second try: fail only during callback
buf = nbd.Buffer (512)
cookie = h.aio_pread_structured (buf, 0,
                                 lambda *args: chunk (42, *args),
                                 lambda *args: callback ((42, 43), *args))
try:
    while not (h.aio_command_completed (cookie)):
        h.poll (-1)
    assert False
except nbd.Error as ex:
    assert ex.errnum == errno.ENOMEM

# Third try: fail during both
buf = nbd.Buffer (512)
cookie = h.aio_pread_structured (buf, 0,
                                 lambda *args: chunk (43, *args),
                                 lambda *args: callback ((43, 43), *args))
try:
    while not (h.aio_command_completed (cookie)):
        h.poll (-1)
    assert False
except nbd.Error as ex:
    assert ex.errnum == errno.ENOMEM

# Fourth try: fail only during chunk
buf = nbd.Buffer (512)
cookie = h.aio_pread_structured (buf, 0,
                                 lambda *args: chunk (43, *args),
                                 lambda *args: callback ((43, 42), *args))
try:
    while not (h.aio_command_completed (cookie)):
        h.poll (-1)
    assert False
except nbd.Error as ex:
    assert ex.errnum == errno.EPROTO
