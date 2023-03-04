#!/usr/bin/env bash
# nbd client library in userspace
# Copyright Red Hat
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA

# Test the implicit handle (h) and the -n option.

. ../tests/functions.sh
set -e

$VG nbdsh -c '
assert h is not None
assert isinstance(h, nbd.NBD)
assert isinstance(h.get_debug(), bool)
'

$VG nbdsh -n -c '
try:
    h
except NameError:
    exit(0)
else:
    import sys
    print("h should not be defined", file=sys.stderr)
    exit(1)
'
