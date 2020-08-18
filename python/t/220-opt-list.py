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

import nbd
import errno
import os

# Require new-enough nbdkit
if os.system ("nbdkit sh --dump-plugin | grep -q has_list_exports=1"):
    print ("skipping: nbdkit too old for this test")
    exit (0)

script = ("%s/../tests/opt-list.sh" % os.getenv ("srcdir", "."))

h = nbd.NBD ()
h.set_opt_mode (True)
h.connect_command (["nbdkit", "-s", "--exit-with-parent", "-v",
                    "sh", script])

exports = []
def f (user_data, name, desc):
    global exports
    assert user_data == 42
    assert desc == ""
    exports.append(name)

# First pass: server fails NBD_OPT_LIST
try:
    h.opt_list (lambda *args: f (42, *args))
    assert False
except nbd.Error as ex:
    pass
assert exports == []

# Second pass: server advertises 'a' and 'b'
exports = []
assert h.opt_list (lambda *args: f (42, *args)) == 2
assert exports == [ "a", "b" ]

# Third pass: server advertises empty list
exports = []
assert h.opt_list (lambda *args: f (42, *args)) == 0
assert exports == []

# Final pass: server advertises 'a'
exports = []
assert h.opt_list (lambda *args: f (42, *args)) == 1
assert exports == [ "a" ]

h.opt_abort ()
