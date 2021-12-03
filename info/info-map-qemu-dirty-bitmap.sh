#!/usr/bin/env bash
# nbd client library in userspace
# Copyright (C) 2020-2021 Red Hat Inc.
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

# Attempt to test using nbdinfo --map=qemu:dirty-bitmap.
# See also interop/dirty-bitmap.{c,sh}

. ../tests/functions.sh

set -e
set -x

requires qemu-img bitmap --help
requires qemu-io --version
requires $QEMU_NBD -B test --pid-file=test.pid --version
requires tr --version

f=info-map-qemu-dirty-bitmap.qcow2
out=info-map-qemu-dirty-bitmap.out
cleanup_fn rm -f $f $out
rm -f $f $out

# Create file with intentionally different written areas vs. dirty areas
qemu-img create -f qcow2 $f 1M
qemu-io -f qcow2 -c 'w 0 64k' $f
qemu-img bitmap --add --enable -f qcow2 $f bitmap0
qemu-io -f qcow2 -c 'w 64k 64k' -c 'w -z 512k 64k' $f

# We have to run qemu-nbd and attempt to clean it up afterwards.
sock=$(mktemp -u /tmp/libnbd-test-info.XXXXXX)
pid=info-map-qemu-dirty-bitmap.pid
cleanup_fn rm -f $sock $pid
rm -f $sock $pid

$QEMU_NBD -t --socket=$sock --pid-file=$pid -f qcow2 -B bitmap0 $f &
cleanup_fn kill $!

wait_for_pidfile qemu-nbd $pid

$VG nbdinfo --map=qemu:dirty-bitmap:bitmap0 "nbd+unix://?socket=$sock" > $out
cat $out

if [ "$(tr -s ' ' < $out)" != " 0 65536 0 clean
 65536 65536 1 dirty
 131072 393216 0 clean
 524288 65536 1 dirty
 589824 458752 0 clean" ]; then
    echo "$0: unexpected output from nbdinfo --map"
    exit 1
fi
