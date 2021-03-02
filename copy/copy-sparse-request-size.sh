#!/usr/bin/env bash
# nbd client library in userspace
# Copyright (C) 2021 Red Hat Inc.
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

# Adapted from copy-sparse-no-extents.sh
#
# This test depends on the nbdkit default sparse block size (32K).

. ../tests/functions.sh

set -e
set -x

# Skip this test under valgrind, it takes too long.
if [ "x$LIBNBD_VALGRIND" = "x1" ]; then
    echo "$0: test skipped under valgrind"
    exit 77
fi

requires nbdkit --version
requires nbdkit --exit-with-parent --version
requires nbdkit data --version
requires nbdkit eval --version

out=copy-sparse-request-size.out
cleanup_fn rm -f $out

$VG nbdcopy --no-extents -S 0 --request-size=1048576 -- \
    [ nbdkit --exit-with-parent data data='
             1
             @33554431 1
             ' ] \
    [ nbdkit --exit-with-parent eval \
             get_size=' echo 33554432 ' \
             pwrite=" echo \$@ >> $out " \
             trim=" echo \$@ >> $out " \
             zero=" echo \$@ >> $out " ]

LC_ALL=C sort -k1,1 -k2,2n -k3,3n -o $out $out

echo Output:
cat $out

if [ "$(cat $out)" != "pwrite 1048576 0
pwrite 1048576 1048576
pwrite 1048576 2097152
pwrite 1048576 3145728
pwrite 1048576 4194304
pwrite 1048576 5242880
pwrite 1048576 6291456
pwrite 1048576 7340032
pwrite 1048576 8388608
pwrite 1048576 9437184
pwrite 1048576 10485760
pwrite 1048576 11534336
pwrite 1048576 12582912
pwrite 1048576 13631488
pwrite 1048576 14680064
pwrite 1048576 15728640
pwrite 1048576 16777216
pwrite 1048576 17825792
pwrite 1048576 18874368
pwrite 1048576 19922944
pwrite 1048576 20971520
pwrite 1048576 22020096
pwrite 1048576 23068672
pwrite 1048576 24117248
pwrite 1048576 25165824
pwrite 1048576 26214400
pwrite 1048576 27262976
pwrite 1048576 28311552
pwrite 1048576 29360128
pwrite 1048576 30408704
pwrite 1048576 31457280
pwrite 1048576 32505856" ]; then
    echo "$0: output does not match expected"
    exit 1
fi
