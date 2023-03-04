# libnbd Python bindings
# Copyright Red Hat
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

script = "%s/../tests/opt-info.sh" % os.getenv("srcdir", ".")


def must_fail(f, *args, **kwds):
    try:
        f(*args, **kwds)
        assert False
    except nbd.Error:
        pass


h = nbd.NBD()
h.set_opt_mode(True)
h.connect_command(["nbdkit", "-s", "--exit-with-parent", "-v", "sh", script])
h.add_meta_context(nbd.CONTEXT_BASE_ALLOCATION)

# No size, flags, or meta-contexts yet
must_fail(h.get_size)
must_fail(h.is_read_only)
must_fail(h.can_meta_context, nbd.CONTEXT_BASE_ALLOCATION)

# info with no prior name gets info on ""
h.opt_info()
assert h.get_size() == 0
assert h.is_read_only() is True
assert h.can_meta_context(nbd.CONTEXT_BASE_ALLOCATION) is True

# changing export wipes out prior info
h.set_export_name("b")
must_fail(h.get_size)
must_fail(h.is_read_only)
must_fail(h.can_meta_context, nbd.CONTEXT_BASE_ALLOCATION)

# info on something not present fails
h.set_export_name("a")
must_fail(h.opt_info)

# info for a different export, with automatic meta_context disabled
h.set_export_name("b")
h.set_request_meta_context(False)
h.opt_info()
# idempotent name change is no-op
h.set_export_name("b")
assert h.get_size() == 1
assert h.is_read_only() is False
must_fail(h.can_meta_context, nbd.CONTEXT_BASE_ALLOCATION)
h.set_request_meta_context(True)

# go on something not present
h.set_export_name("a")
must_fail(h.opt_go)
must_fail(h.get_size)
must_fail(h.is_read_only)
must_fail(h.can_meta_context, nbd.CONTEXT_BASE_ALLOCATION)

# go on a valid export
h.set_export_name("good")
h.opt_go()
assert h.get_size() == 4
assert h.is_read_only() is True
assert h.can_meta_context(nbd.CONTEXT_BASE_ALLOCATION) is True

# now info is no longer valid, but does not wipe data
must_fail(h.set_export_name, "a")
assert h.get_export_name() == "good"
must_fail(h.opt_info)
assert h.get_size() == 4
assert h.can_meta_context(nbd.CONTEXT_BASE_ALLOCATION) is True

h.shutdown()

# Another connection. This time, check that SET_META triggered by opt_info
# persists through nbd_opt_go with set_request_meta_context disabled.
h = nbd.NBD()
h.set_opt_mode(True)
h.connect_command(["nbdkit", "-s", "--exit-with-parent", "-v", "sh", script])
h.add_meta_context("x-unexpected:bogus")

must_fail(h.can_meta_context, nbd.CONTEXT_BASE_ALLOCATION)
h.opt_info()
assert h.can_meta_context(nbd.CONTEXT_BASE_ALLOCATION) is False
h.set_request_meta_context(False)
# Adding to the request list now won't matter
h.add_meta_context(nbd.CONTEXT_BASE_ALLOCATION)
h.opt_go()
assert h.can_meta_context(nbd.CONTEXT_BASE_ALLOCATION) is False

h.shutdown()
