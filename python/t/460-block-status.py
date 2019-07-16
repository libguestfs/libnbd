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

script = ("%s/../tests/meta-base-allocation.sh" % os.getenv ("srcdir", "."))

h = nbd.NBD ()
h.add_meta_context ("base:allocation")
h.connect_command (["nbdkit", "-s", "--exit-with-parent", "-v",
                    "sh", script])

entries = []
def f (user_data, metacontext, offset, e, err):
    global entries
    assert user_data == 42
    assert err.value == 0
    if metacontext != "base:allocation":
        return
    entries = e

h.block_status (65536, 0, lambda *args: f (42, *args))
assert entries == [ 8192, 0,
                    8192, 1,
                   16384, 3,
                   16384, 2,
                   16384, 0]

h.block_status (1024, 32256, lambda *args: f (42, *args))
print ("entries = %r" % entries)
assert entries == [  512, 3,
                   16384, 2]

h.block_status (1024, 32256, lambda *args: f (42, *args),
                nbd.CMD_FLAG_REQ_ONE)
print ("entries = %r" % entries)
assert entries == [  512, 3]
