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

import gc
import re
import nbd

# Catch debug messages so we know when the handle was really closed.
messages = []


def f(context, msg):
    global messages

    messages.append(msg)


# Open the handle and then explicitly close it.
h = nbd.NBD()
h.set_debug(True)
h.set_debug_callback(f)
h.close()

# Check the messages so we know the handle was closed.
matches = [msg for msg in messages if re.match("closing", msg)]
assert len(matches) == 1

# Check that an exception is raised if we use any method on h.
try:
    h.set_export_name("test")
    assert False
except nbd.ClosedHandle:
    # Expected.
    pass
except Exception as exn:
    print("unexpected exception: %r" % exn)
    assert False

try:
    h.close()
    assert False
except nbd.ClosedHandle:
    # Expected.
    pass
except Exception as exn:
    print("unexpected exception: %r" % exn)
    assert False

gc.collect()

# Check there are no more closing handle messages (which is probably
# impossible).
matches = [msg for msg in messages if re.match("closing", msg)]
assert len(matches) == 1
