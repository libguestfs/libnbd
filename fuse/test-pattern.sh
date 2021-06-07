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

# Use nbdkit-pattern-plugin to test we can read from the correct place
# in the file, essentially a test of data integrity.

. ../tests/functions.sh

set -e
set -x

requires_fuse
requires dd --version
requires dd iflag=count_bytes,skip_bytes </dev/null
requires hexdump -C /dev/null
requires nbdkit --version
requires nbdkit pattern --version

# Difficult to arrange for this test to be run this test under
# valgrind, so don't bother.
if [ "x$LIBNBD_VALGRIND" = "x1" ]; then
    echo "$0: test skipped under valgrind"
    exit 77
fi

mp=test-pattern.d
out=test-pattern.out
pidfile=test-pattern.pid
cleanup_fn fusermount3 -u $mp
cleanup_fn rm -rf $mp $out $pidfile

mkdir $mp
$VG nbdfuse -P $pidfile $mp [ nbdkit --exit-with-parent pattern size=1G ] &

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

# Read from various places in the file and ensure what we read is
# correct.
dd if=$mp/nbd skip=4096 count=128 iflag=count_bytes,skip_bytes |
    hexdump -C > $out
cat $out
if [ "$(cat $out)" != '00000000  00 00 00 00 00 00 10 00  00 00 00 00 00 00 10 08  |................|
00000010  00 00 00 00 00 00 10 10  00 00 00 00 00 00 10 18  |................|
00000020  00 00 00 00 00 00 10 20  00 00 00 00 00 00 10 28  |....... .......(|
00000030  00 00 00 00 00 00 10 30  00 00 00 00 00 00 10 38  |.......0.......8|
00000040  00 00 00 00 00 00 10 40  00 00 00 00 00 00 10 48  |.......@.......H|
00000050  00 00 00 00 00 00 10 50  00 00 00 00 00 00 10 58  |.......P.......X|
00000060  00 00 00 00 00 00 10 60  00 00 00 00 00 00 10 68  |.......`.......h|
00000070  00 00 00 00 00 00 10 70  00 00 00 00 00 00 10 78  |.......p.......x|
00000080' ]; then
    echo "$0: error: pattern data did not match expected"
    exit 1
fi

dd if=$mp/nbd skip=1000000 count=128 iflag=count_bytes,skip_bytes |
    hexdump -C > $out
cat $out
if [ "$(cat $out)" != '00000000  00 00 00 00 00 0f 42 40  00 00 00 00 00 0f 42 48  |......B@......BH|
00000010  00 00 00 00 00 0f 42 50  00 00 00 00 00 0f 42 58  |......BP......BX|
00000020  00 00 00 00 00 0f 42 60  00 00 00 00 00 0f 42 68  |......B`......Bh|
00000030  00 00 00 00 00 0f 42 70  00 00 00 00 00 0f 42 78  |......Bp......Bx|
00000040  00 00 00 00 00 0f 42 80  00 00 00 00 00 0f 42 88  |......B.......B.|
00000050  00 00 00 00 00 0f 42 90  00 00 00 00 00 0f 42 98  |......B.......B.|
00000060  00 00 00 00 00 0f 42 a0  00 00 00 00 00 0f 42 a8  |......B.......B.|
00000070  00 00 00 00 00 0f 42 b0  00 00 00 00 00 0f 42 b8  |......B.......B.|
00000080' ]; then
    echo "$0: error: pattern data did not match expected"
    exit 1
fi

dd if=$mp/nbd skip=0 count=128 iflag=count_bytes,skip_bytes |
    hexdump -C > $out
cat $out
if [ "$(cat $out)" != '00000000  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 08  |................|
00000010  00 00 00 00 00 00 00 10  00 00 00 00 00 00 00 18  |................|
00000020  00 00 00 00 00 00 00 20  00 00 00 00 00 00 00 28  |....... .......(|
00000030  00 00 00 00 00 00 00 30  00 00 00 00 00 00 00 38  |.......0.......8|
00000040  00 00 00 00 00 00 00 40  00 00 00 00 00 00 00 48  |.......@.......H|
00000050  00 00 00 00 00 00 00 50  00 00 00 00 00 00 00 58  |.......P.......X|
00000060  00 00 00 00 00 00 00 60  00 00 00 00 00 00 00 68  |.......`.......h|
00000070  00 00 00 00 00 00 00 70  00 00 00 00 00 00 00 78  |.......p.......x|
00000080' ]; then
    echo "$0: error: pattern data did not match expected"
    exit 1
fi
