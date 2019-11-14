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

# Test nbdfuse + nbdkit.

. ../tests/functions.sh

set -e
set -x

requires nbdkit --exit-with-parent --version
requires cmp --version
requires dd --version

if ! test -r /dev/urandom; then
    echo "$0: test skipped: /dev/urandom not readable"
    exit 77
fi

pidfile=test-nbdkit.pid
mp=test-nbdkit.d
data=test-nbdkit.data
cleanup_fn fusermount -u $mp
cleanup_fn rm -rf $mp
cleanup_fn rm -f $pidfile $data

mkdir -p $mp
$VG nbdfuse -P $pidfile $mp \
        --command nbdkit -s --exit-with-parent memory size=10M &

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

dd if=/dev/urandom of=$data bs=1M count=10
# Use a weird block size when writing.  It's a bit pointless because
# something in the Linux/FUSE stack turns these into exact 4096 byte
# writes.
dd if=$data of=$mp/nbd bs=65519 conv=nocreat,notrunc
cmp $data $mp/nbd
