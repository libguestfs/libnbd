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

requires_root
requires_caps cap_sys_admin
requires nbdkit --exit-with-parent --version
requires test -r /sys/module/nbd
requires nbd-client --version
# /dev/nbd0 must not be in use.
requires_not nbd-client -c /dev/nbd0

pidfile=copy-block-to-nbd.pid
sock=$(mktemp -u /tmp/libnbd-test-copy.XXXXXX)
cleanup_fn rm -f $pidfile $sock
cleanup_fn nbd-client -d /dev/nbd0

# Run an nbdkit server to act as the backing for /dev/nbd0.
nbdkit --exit-with-parent -f -v -P $pidfile -U $sock pattern size=5M &
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

nbd-client -unix $sock /dev/nbd0 -b 512

$VG nbdcopy -- /dev/nbd0 [ nbdkit --exit-with-parent -v memory 5M ]
