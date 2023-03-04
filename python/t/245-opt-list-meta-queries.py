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


count = 0
seen = False


def f(user_data, name):
    global count
    global seen
    assert user_data == 42
    count = count + 1
    if name == nbd.CONTEXT_BASE_ALLOCATION:
        seen = True


# Get into negotiating state.
h = nbd.NBD()
h.set_opt_mode(True)
h.connect_command(["nbdkit", "-s", "--exit-with-parent", "-v",
                   "memory", "size=1M"])

# First pass: empty query should give at least "base:allocation".
# The explicit query overrides a non-empty nbd_add_meta_context.
count = 0
seen = False
h.add_meta_context("x-nosuch:")
r = h.opt_list_meta_context_queries([], lambda *args: f(42, *args))
assert r == count
assert r >= 1
assert seen is True

# Second pass: bogus query has no response.
count = 0
seen = False
h.clear_meta_contexts()
r = h.opt_list_meta_context_queries(["x-nosuch:"], lambda *args: f(42, *args))
assert r == 0
assert r == count
assert seen is False

# Third pass: specific query should have one match.
count = 0
seen = False
r = h.opt_list_meta_context_queries(["x-nosuch:", nbd.CONTEXT_BASE_ALLOCATION],
                                    lambda *args: f(42, *args))
assert r == 1
assert count == 1
assert seen is True

h.opt_abort()
