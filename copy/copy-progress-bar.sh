#!/usr/bin/env bash
# nbd client library in userspace
# Copyright (C) 2020 Red Hat Inc.
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

. ../tests/functions.sh

set -e
set -x

requires dd --version
requires test -r /dev/urandom

file=copy-progress-bar.file
file2=copy-progress-bar.file2
file3=copy-progress-bar.file3
cleanup_fn rm -f $file $file2 $file3

dd if=/dev/urandom of=$file bs=512 count=1

# Check that a regular progress bar works.
# This writes to /dev/tty :-)
$VG nbdcopy --progress $file $file2

# Check that a machine-readable progress bar works.
exec 3>$file3
$VG nbdcopy --progress=3 $file $file2
exec 3>&-

cat $file3

# 100/100 must always be printed.
grep "100/100" $file3
