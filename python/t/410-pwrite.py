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

buf1 = bytearray (512)
buf1[10] = 1
buf1[510] = 0x55
buf1[511] = 0xAA

datafile = "410-pwrite.data"

with open (datafile, "wb") as f:
    f.truncate (512)

h = nbd.NBD ()
h.connect_command (["nbdkit", "-s", "--exit-with-parent", "-v",
                    "file", datafile])
h.pwrite (buf1, 0, nbd.CMD_FLAG_FUA)
buf2 = h.pread (512, 0)

assert buf1 == buf2

with open (datafile, "rb") as f:
    content = f.read ()

assert buf1 == content

os.unlink (datafile)
