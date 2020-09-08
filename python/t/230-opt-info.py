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
import os

script = ("%s/../tests/opt-info.sh" % os.getenv ("srcdir", "."))


def must_fail (f, *args, **kwds):
    try:
        f (*args, **kwds)
        assert False
    except nbd.Error:
        pass


h = nbd.NBD ()
h.set_opt_mode (True)
h.connect_command (["nbdkit", "-s", "--exit-with-parent", "-v",
                    "sh", script])
h.add_meta_context (nbd.CONTEXT_BASE_ALLOCATION)

# No size, flags, or meta-contexts yet */
must_fail (h.get_size)
must_fail (h.is_read_only)
must_fail (h.can_meta_context, nbd.CONTEXT_BASE_ALLOCATION)

# info with no prior name gets info on ""
h.opt_info ()
assert h.get_size () == 0
assert h.is_read_only () == 1
assert h.can_meta_context (nbd.CONTEXT_BASE_ALLOCATION) == 1

# info on something not present fails, wipes out prior info
h.set_export_name ("a")
must_fail (h.opt_info)
must_fail (h.get_size)
must_fail (h.is_read_only)
must_fail (h.can_meta_context, nbd.CONTEXT_BASE_ALLOCATION)

# info for a different export
h.set_export_name ("b")
h.opt_info ()
assert h.get_size () == 1
assert h.is_read_only () == 0
assert h.can_meta_context (nbd.CONTEXT_BASE_ALLOCATION) == 1

# go on something not present
h.set_export_name ("a")
must_fail (h.opt_go)
must_fail (h.get_size)
must_fail (h.is_read_only)
must_fail (h.can_meta_context, nbd.CONTEXT_BASE_ALLOCATION)

# go on a valid export
h.set_export_name ("good")
h.opt_go ()
assert h.get_size () == 4
assert h.is_read_only () == 1
assert h.can_meta_context (nbd.CONTEXT_BASE_ALLOCATION) == 1

# now info is no longer valid, but does not wipe data
must_fail (h.set_export_name, "a")
assert h.get_export_name () == "good"
must_fail (h.opt_info)
assert h.get_size () == 4

h.shutdown ()
