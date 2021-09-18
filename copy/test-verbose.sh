#!/usr/bin/env bash
# nbd client library in userspace
# Copyright (C) 2019-2021 Red Hat Inc.
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

# Test that nbdcopy -v/--verbose looks sane.

. ../tests/functions.sh

set -e
set -x

requires nbdkit --version

file=test-verbose.out
cleanup_fn rm -f $file

$VG nbdcopy -v -- [ nbdkit memory 1M ] null: 2>$file

cat $file

# Check some known strings appear in the output.
grep '^nbdcopy: src: nbd_ops' $file
grep '^nbdcopy: src: size=1048576 (1M)' $file
grep '^nbdcopy: dst: null_ops' $file
