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


count = 0
seen = False


def f(user_data, name):
    global count
    global seen
    assert user_data == 42
    count = count + 1
    if name == nbd.CONTEXT_BASE_ALLOCATION:
        seen = True


def must_fail(f, *args, **kwds):
    try:
        f(*args, **kwds)
        assert False
    except nbd.Error:
        pass


# Get into negotiating state without structured replies.
h = nbd.NBD()
h.set_opt_mode(True)
h.set_request_structured_replies(False)
h.connect_command(["nbdkit", "-s", "--exit-with-parent", "-v",
                   "memory", "size=1M"])

# No contexts negotiated yet; can_meta should be error if any requested
assert h.get_structured_replies_negotiated() is False
assert h.can_meta_context(nbd.CONTEXT_BASE_ALLOCATION) is False
h.add_meta_context(nbd.CONTEXT_BASE_ALLOCATION)
must_fail(h.can_meta_context, nbd.CONTEXT_BASE_ALLOCATION)

# SET cannot succeed until SR is negotiated.
count = 0
seen = False
must_fail(h.opt_set_meta_context, lambda *args: f(42, *args))
assert count == 0
assert seen is False
assert h.opt_structured_reply() is True
assert h.get_structured_replies_negotiated() is True
must_fail(h.can_meta_context, nbd.CONTEXT_BASE_ALLOCATION)

# nbdkit does not match wildcard for SET, even though it does for LIST
count = 0
seen = False
h.clear_meta_contexts()
h.add_meta_context("base:")
r = h.opt_set_meta_context(lambda *args: f(42, *args))
assert r == count
assert r == 0
assert seen is False
assert h.can_meta_context(nbd.CONTEXT_BASE_ALLOCATION) is False

# Negotiating with no contexts is not an error, but selects nothing
count = 0
seen = False
h.clear_meta_contexts()
r = h.opt_set_meta_context(lambda *args: f(42, *args))
assert r == 0
assert r == count
assert seen is False
assert h.can_meta_context(nbd.CONTEXT_BASE_ALLOCATION) is False

# Request 2 with expectation of 1; with set_request_meta_context off
count = 0
seen = False
h.add_meta_context("x-nosuch:context")
h.add_meta_context(nbd.CONTEXT_BASE_ALLOCATION)
h.set_request_meta_context(False)
r = h.opt_set_meta_context(lambda *args: f(42, *args))
assert r == 1
assert count == 1
assert seen is True
assert h.can_meta_context(nbd.CONTEXT_BASE_ALLOCATION) is True

# Transition to transmission phase; our last set should remain active
h.clear_meta_contexts()
h.add_meta_context("x-nosuch:context")
h.opt_go()
assert h.can_meta_context(nbd.CONTEXT_BASE_ALLOCATION) is True

# Now too late to set; but should not lose earlier state
count = 0
seen = False
must_fail(h.opt_set_meta_context, lambda *args: f(42, *args))
assert count == 0
assert seen is False
assert h.can_meta_context(nbd.CONTEXT_BASE_ALLOCATION) is True

h.shutdown()
