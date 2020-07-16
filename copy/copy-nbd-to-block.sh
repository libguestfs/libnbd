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
requires nbdkit --exit-with-parent --version
requires nbd-client --version
# /dev/nbd0 must not be in use.
requires_not nbd-client -c /dev/nbd0

pidfile1=copy-nbd-to-block.pid
pidfile2=copy-nbd-to-block.pid
sock1=`mktemp -u`
sock2=`mktemp -u`
cleanup_fn rm -f $pidfile1 $pidfile2 $sock1 $sock2
cleanup_fn nbd-client -d /dev/nbd0

# We have two nbdkit servers, one is the source and the other is the
# destination.

nbdkit --exit-with-parent -f -v -P $pidfile1 -U $sock1 pattern 5M &
nbdkit --exit-with-parent -f -v -P $pidfile2 -U $sock2 memory 5M &
# Wait for the pidfiles to appear.
for i in {1..60}; do
    if test -f $pidfile1 && test -f $pidfile2; then
        break
    fi
    sleep 1
done
if ! test -f $pidfile1 || ! test -f $pidfile2; then
    echo "$0: nbdkit did not start up"
    exit 1
fi

nbd-client -unix $sock2 /dev/nbd0 -b 512

$VG nbdcopy "nbd+unix:///?socket=$sock1" /dev/nbd0

# Check that /dev/nbd0 is still a block device and we didn't
# accidentally overwrite it with a regular file.
test -b /dev/nbd0
