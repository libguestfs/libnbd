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
requires stat --version

pidfile1=copy-nbd-to-nbd.pid1
pidfile2=copy-nbd-to-nbd.pid2
file1=copy-nbd-to-nbd.file1
file2=copy-nbd-to-nbd.file2
sock1=$(mktemp -u /tmp/libnbd-test-copy.XXXXXX)
sock2=$(mktemp -u /tmp/libnbd-test-copy.XXXXXX)
cleanup_fn rm -f $pidfile1 $pidfile2 $file1 $file2 $sock1 $sock2

nbdkit --exit-with-parent -f -v -P $pidfile1 -U $sock1 pattern size=10M &
# Wait for the pidfile to appear.
for i in {1..60}; do
    if test -f $pidfile1; then
        break
    fi
    sleep 1
done
if ! test -f $pidfile1; then
    echo "$0: nbdkit did not start up"
    exit 1
fi

nbdkit --exit-with-parent -f -v -P $pidfile2 -U $sock2 memory size=10M &
# Wait for the pidfile to appear.
for i in {1..60}; do
    if test -f $pidfile2; then
        break
    fi
    sleep 1
done
if ! test -f $pidfile2; then
    echo "$0: nbdkit did not start up"
    exit 1
fi

$VG nbdcopy "nbd+unix:///?socket=$sock1" "nbd+unix:///?socket=$sock2"

# Download the file from both servers and check they are the same.
$VG nbdcopy "nbd+unix:///?socket=$sock1" $file1
$VG nbdcopy "nbd+unix:///?socket=$sock2" $file2
cmp $file1 $file2

# Test the data is at least non-zero.
test "$(hexdump -C $file1 | head -1)" = "00000000  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 08  |................|"
