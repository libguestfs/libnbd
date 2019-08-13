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

import os

import nbd

buf = bytearray (512)
buf[10] = 1
buf[510] = 0x55
buf[511] = 0xAA

datafile = "510-pwrite.data"

with open (datafile, "wb") as f:
    f.truncate (512)

h = nbd.NBD ()
h.connect_command (["nbdkit", "-s", "--exit-with-parent", "-v",
                    "file", datafile])

buf1 = nbd.Buffer.from_bytearray (buf)
cookie = h.aio_pwrite (buf1, 0, flags=nbd.CMD_FLAG_FUA)
while not (h.aio_command_completed (cookie)):
    h.poll (-1)

buf2 = nbd.Buffer (512)
cookie = h.aio_pread (buf2, 0)
while not (h.aio_command_completed (cookie)):
    h.poll (-1)

assert buf == buf2.to_bytearray ()

with open (datafile, "rb") as f:
    content = f.read ()

assert buf == content

os.unlink (datafile)
