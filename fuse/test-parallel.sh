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

# Test parallel reads and writes to the fuse file.

. ../tests/functions.sh

set -e
set -x

requires_fuse
requires dd --version
requires nbdkit --version
requires nbdkit memory --version

# Difficult to arrange for this test to be run this test under
# valgrind, so don't bother.
if [ "x$LIBNBD_VALGRIND" = "x1" ]; then
    echo "$0: test skipped under valgrind"
    exit 77
fi

mp=test-parallel.d
pidfile=test-parallel.pid
cleanup_fn fusermount3 -u $mp
cleanup_fn rm -rf $mp $pidfile

mkdir $mp
$VG nbdfuse -P $pidfile $mp [ nbdkit --exit-with-parent memory size=10M ] &

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

ls -al $mp

declare -a pids
for f in 1 2 3 4; do dd if=$mp/nbd of=/dev/null & pids+=($!) ; done
for f in 1 2 3 4; do dd if=/dev/zero of=$mp/nbd bs=1M count=10 & pids+=($!) ; done
wait ${pids[@]}
