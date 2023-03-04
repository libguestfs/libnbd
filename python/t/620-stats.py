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

h = nbd.NBD()

# Pre-connection, stats start out at 0
bs0 = h.stats_bytes_sent()
cs0 = h.stats_chunks_sent()
br0 = h.stats_bytes_received()
cr0 = h.stats_chunks_received()

assert bs0 == 0
assert cs0 == 0
assert br0 == 0
assert cr0 == 0

# Connection performs handshaking, which increments stats.
# The number of bytes/chunks here may grow over time as more features get
# automatically negotiated, so merely check that they are non-zero.
h.connect_command(["nbdkit", "-s", "--exit-with-parent", "null"])

bs1 = h.stats_bytes_sent()
cs1 = h.stats_chunks_sent()
br1 = h.stats_bytes_received()
cr1 = h.stats_chunks_received()

assert cs1 > 0
assert bs1 > cs1
assert cr1 > 0
assert br1 > cr1

# A flush command should be one chunk out, one chunk back (even if
# structured replies are in use)
h.flush()

bs2 = h.stats_bytes_sent()
cs2 = h.stats_chunks_sent()
br2 = h.stats_bytes_received()
cr2 = h.stats_chunks_received()

assert bs2 == bs1 + 28
assert cs2 == cs1 + 1
assert br2 == br1 + 16   # assumes nbdkit uses simple reply
assert cr2 == cr1 + 1

# Stats are still readable after the connection closes; we don't know if
# the server sent reply bytes to our NBD_CMD_DISC, so don't insist on it.
h.shutdown()

bs3 = h.stats_bytes_sent()
cs3 = h.stats_chunks_sent()
br3 = h.stats_bytes_received()
cr3 = h.stats_chunks_received()

assert bs3 > bs2
assert cs3 == cs2 + 1
assert br3 >= br2
assert cr3 == cr2 + (br3 > br2)

# Try to trigger garbage collection of h
h = None
