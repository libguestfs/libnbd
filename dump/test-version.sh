#!/usr/bin/env bash
# nbd client library in userspace
# Copyright (C) 2019 Red Hat Inc.
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

# Test that nbddump --version looks sane.

fail=0
output=$($VG nbddump --version)
if [ $? != 0 ]; then
    echo "$0: unexpected exit status"
    fail=1
fi
if [ "$output" != "nbddump $EXPECTED_VERSION
libnbd $EXPECTED_VERSION" ]; then
    echo "$0: unexpected output"
    fail=1
fi
echo "$output"
exit $fail
