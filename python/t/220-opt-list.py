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

# First pass: server fails NBD_OPT_LIST
# XXX We can't tell the difference
h.opt_list ()
assert h.get_nr_list_exports () == 0

# Second pass: server advertises 'a' and 'b'
h.opt_list ()
assert h.get_nr_list_exports () == 2
assert h.get_list_export_name (0) == "a"
assert h.get_list_export_name (1) == "b"

# Third pass: server advertises empty list
h.opt_list ()
assert h.get_nr_list_exports () == 0
try:
    h.get_list_export_name (0)
    assert False
except nbd.Error as ex:
    pass

# Final pass: server advertises 'a'
h.opt_list ()
assert h.get_nr_list_exports () == 1
assert h.get_list_export_name (0) == "a"

h.opt_abort ()
