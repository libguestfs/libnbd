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

# Test trimming of files using nbdkit sh plugin.

. ../tests/functions.sh

set -e
set -x

requires fusermount3 --version
requires fallocate --version
requires dd --version
requires nbdkit --version
requires nbdkit sh --version

# Difficult to arrange for this test to be run this test under
# valgrind, so don't bother.
if [ "x$LIBNBD_VALGRIND" = "x1" ]; then
    echo "$0: test skipped under valgrind"
    exit 77
fi

mp=test-trim.d
pidfile=test-trim.pid
out=test-trim.out
cleanup_fn fusermount3 -u $mp
cleanup_fn rm -rf $mp $pidfile $out

mkdir $mp

export mp pidfile out
nbdkit -U - \
       sh - \
       --run '
set -x

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
    echo "$0: nbdfuse PID file $pidfile was not created"
    exit 1
fi

ls -al $mp

# Fully allocate the disk.
dd if=/dev/zero of=$mp/nbd bs=512 count=2

# Punch a hole in the second sector.
fallocate -p -l 512 -o 512 $mp/nbd

# We have to explicitly unmount it here else nbdfuse will
# never exit and nbdkit will hang.
fusermount3 -u $mp
' <<'EOF'
# The nbdkit server script.
case "$1" in
  get_size) echo 1024 ;;
  can_write) ;;
  can_trim) ;;
  can_zero) ;;
  pread) ;;
  pwrite) echo "$@" >> $out ;;
  trim) echo "$@" >> $out ;;
  zero) echo "$@" >> $out ;;
  *) exit 2 ;;
esac
EOF

# What commands were sent to nbdkit?
cat $out

grep 'pwrite  512 0' $out
grep 'pwrite  512 512' $out
grep 'trim  512 512' $out
