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

requires nbdkit --exit-with-parent --version
requires cmp --version
requires dd --version
requires test -r /dev/urandom
requires truncate --version

file=copy-file-to-nbd.file
file2=copy-file-to-nbd.file2
pidfile=copy-file-to-nbd.pid
sock=$(mktemp -u /tmp/libnbd-test-copy.XXXXXX)
cleanup_fn rm -f $file $file2 $pidfile $sock

nbdkit --exit-with-parent -f -v -P $pidfile -U $sock memory size=10M &
# Wait for the pidfile to appear.
for i in {1..60}; do
    if test -f $pidfile; then
        break
    fi
    sleep 1
done
if ! test -f $pidfile; then
    echo "$0: nbdkit did not start up"
    exit 1
fi

dd if=/dev/urandom of=$file bs=1M count=10
$VG nbdcopy $file "nbd+unix:///?socket=$sock"
$VG nbdcopy "nbd+unix:///?socket=$sock" $file2

ls -l $file $file2
cmp $file $file2
