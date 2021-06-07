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

# Test nbdfuse --unix mode.

. ../tests/functions.sh

set -e
set -x

requires_fuse
requires nbdkit --exit-with-parent --version

# Difficult to arrange for this test to be run this test under
# valgrind, so don't bother.
if [ "x$LIBNBD_VALGRIND" = "x1" ]; then
    echo "$0: test skipped under valgrind"
    exit 77
fi

mp=test-unix.d
pidfile=test-unix.pid
cleanup_fn fusermount3 -u $mp
cleanup_fn rm -rf $mp $pidfile

mkdir $mp

# Run nbdkit with the null plugin using a Unix domain socket, then
# connect nbdfuse to the socket using --unix.
export mp pidfile prog="$0"
nbdkit -U - null --run '
    nbdfuse -P $pidfile $mp --unix "$unixsocket" &

    # Wait for the pidfile to appear.
    for i in {1..60}; do
        if test -f $pidfile; then
            break
        fi
        sleep 1
    done
    if ! test -f $pidfile; then
        echo "$prog: nbdfuse PID file $pidfile was not created"
        exit 1
    fi

    ls -al $mp

    # We have to explicitly unmount it here else nbdfuse will
    # never exit and nbdkit will hang.
    fusermount3 -u $mp
'
