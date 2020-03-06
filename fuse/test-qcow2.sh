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

# The nbdfuse documentation describes how you can use nbdfuse +
# qemu-nbd to open qcow2 files.  This claim is tested here.

. ../tests/functions.sh

set -e
set -x

requires qemu-nbd --version
requires qemu-img --version
requires cmp --version
requires dd --version

if ! test -r /dev/urandom; then
    echo "$0: test skipped: /dev/urandom not readable"
    exit 77
fi

pidfile=test-qcow2.pid
mp=test-qcow2.d
data=test-qcow2.data
qcow2=test-qcow2.qcow2
cleanup_fn fusermount -u $mp
cleanup_fn rm -rf $mp
cleanup_fn rm -f $pidfile $data $qcow2

dd if=/dev/urandom of=$data bs=1M count=1
qemu-img convert -f raw $data -O qcow2 $qcow2

rm -rf $mp
mkdir -p $mp
$VG nbdfuse -r -P $pidfile $mp \
        --socket-activation qemu-nbd -f qcow2 $qcow2 &

# Wait for the pidfile to appear.
for i in {1..60}; do
    if test -f $pidfile; then
        break
    fi
    sleep 1
done
if ! test -f $pidfile; then
    echo "$0: nbdfuse PID file $pidfile was not created"
    exit 1
fi

cmp $data $mp/nbd
