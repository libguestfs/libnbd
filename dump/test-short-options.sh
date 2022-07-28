#!/usr/bin/env bash
# nbd client library in userspace
# Copyright (C) 2019-2022 Red Hat Inc.
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

# Test that nbddump --short-options looks sane.

. ../tests/functions.sh
set -e
set -x

output=test-short-options.out
cleanup_fn rm -f $output

$VG nbddump --short-options > $output
if [ $? != 0 ]; then
    echo "$0: unexpected exit status"
    fail=1
fi
cat $output
grep -- -n $output
grep -- -V $output
