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

# Test that NBD errors are reported correctly.

. ../tests/functions.sh

set -e
set -x

requires_fuse
requires dd --version
requires dd iflag=count_bytes,skip_bytes </dev/null
requires nbdkit --version
requires nbdkit sh --version

# Difficult to arrange for this test to be run this test under
# valgrind, so don't bother.
if [ "x$LIBNBD_VALGRIND" = "x1" ]; then
    echo "$0: test skipped under valgrind"
    exit 77
fi

mp=test-errors.d
pidfile=test-errors.pid
cleanup_fn fusermount3 -u $mp
cleanup_fn rm -rf $mp $pidfile

mkdir $mp

export mp pidfile prog="$0"
nbdkit -U - \
       sh - \
       --run '
set -e
set -x

error=

# Run nbdfuse and connect to the nbdkit server.
nbdfuse -P $pidfile $mp --unix $unixsocket &

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

# Commands which do not touch byte 100,000 should succeed, but
# watch out for readahead.
dd if=$mp/nbd of=/dev/null skip=500000 count=1000 iflag=count_bytes,skip_bytes
dd if=$mp/nbd of=/dev/null skip=700000 count=200 iflag=count_bytes,skip_bytes

# Commands which touch byte 100,000 must fail.
if dd if=$mp/nbd of=/dev/null skip=99000 count=2000 iflag=count_bytes,skip_bytes; then
    echo "$prog: error: expected dd to fail"
    error=1
fi
if dd if=$mp/nbd of=/dev/null skip=90000 count=12000 iflag=count_bytes,skip_bytes; then
    echo "$prog: error: expected dd to fail"
    error=1
fi

# We have to explicitly unmount it here else nbdfuse will
# never exit and nbdkit will hang.
fusermount3 -u $mp

if [ -z "$error" ]; then exit 0; else exit 1; fi
' <<'EOF'
#!/bin/bash -
# NBD server that has an error at byte 100,000
case "$1" in
  get_size) echo 1M ;;
  pread)
    if [ $4 -le 100000 ] && [ $(( $4 + $3 )) -gt 100000 ]; then
        echo EIO Bad block >&2
        exit 1
    else
        dd if=/dev/zero count=$3 iflag=count_bytes
    fi ;;
  *) exit 2 ;;
esac
EOF
