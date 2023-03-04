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

# Attempt to test using nbdinfo --map=qemu:dirty-bitmap.
# See also interop/dirty-bitmap.{c,sh}

. ../tests/functions.sh

set -e
set -x

requires qemu-img --version
requires qemu-io --version
requires $QEMU_NBD -A --pid-file=test.pid --version
requires tr --version

base=info-map-qemu-allocation-depth
f1=$base.1.qcow2
f2=$base.2.qcow2
f3=$base.3.qcow2
f4=$base.4.qcow2
out=$base.out
cleanup_fn rm -f $f1 $f2 $f3 $f4 $out
rm -f $f1 $f2 $f3 $f4 $out

# Create chain of files:
#    f1: XXXX----
#    f2: --XXXX--
#    f3: -XX--XX-
#    f4: -X---X--
# depth: 41233120
qemu-img create -f qcow2 $f1 8M
qemu-io -f qcow2 -c 'w 0 4M' $f1
qemu-img create -f qcow2 -b $f1 -F qcow2 $f2
qemu-io -f qcow2 -c 'w 2M 4M' $f2
qemu-img create -f qcow2 -b $f2 -F qcow2 $f3
qemu-io -f qcow2 -c 'w 1M 2M' -c 'w 5M 2M' $f3
qemu-img create -f qcow2 -b $f3 -F qcow2 $f4
qemu-io -f qcow2 -c 'w 1M 1M' -c 'w 5M 1M' $f4

# We have to run qemu-nbd and attempt to clean it up afterwards.
sock=$(mktemp -u /tmp/libnbd-test-info.XXXXXX)
pid=$base.pid
cleanup_fn rm -f $sock $pid
rm -f $sock $pid

$QEMU_NBD -t --socket=$sock --pid-file=$pid -f qcow2 -A $f4 &
cleanup_fn kill $!

wait_for_pidfile qemu-nbd $pid

$VG nbdinfo --map=qemu:allocation-depth "nbd+unix://?socket=$sock" > $out
cat $out

if [ "$(tr -s ' ' < $out)" != "\
 0 1048576 4 backing depth 4
 1048576 1048576 1 local
 2097152 1048576 2 backing depth 2
 3145728 2097152 3 backing depth 3
 5242880 1048576 1 local
 6291456 1048576 2 backing depth 2
 7340032 1048576 0 absent" ]; then
    echo "$0: unexpected output from nbdinfo --map"
    exit 1
fi
