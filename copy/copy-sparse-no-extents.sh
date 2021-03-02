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

# Adapted from copy-sparse.sh
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

out=copy-sparse-no-extents.out
cleanup_fn rm -f $out

$VG nbdcopy --no-extents -S 0 -- \
    [ nbdkit --exit-with-parent data data='
             1
             @1073741823 1
             ' ] \
    [ nbdkit --exit-with-parent eval \
             get_size=' echo 7E ' \
             pwrite=" echo \$@ >> $out " \
             trim=" echo \$@ >> $out " \
             zero=" echo \$@ >> $out " ]

LC_ALL=C sort -k1,1 -k2,2n -k3,3n -o $out $out

echo Output:
cat $out

if [ "$(cat $out)" != "pwrite 33554432 0
pwrite 33554432 33554432
pwrite 33554432 67108864
pwrite 33554432 100663296
pwrite 33554432 134217728
pwrite 33554432 167772160
pwrite 33554432 201326592
pwrite 33554432 234881024
pwrite 33554432 268435456
pwrite 33554432 301989888
pwrite 33554432 335544320
pwrite 33554432 369098752
pwrite 33554432 402653184
pwrite 33554432 436207616
pwrite 33554432 469762048
pwrite 33554432 503316480
pwrite 33554432 536870912
pwrite 33554432 570425344
pwrite 33554432 603979776
pwrite 33554432 637534208
pwrite 33554432 671088640
pwrite 33554432 704643072
pwrite 33554432 738197504
pwrite 33554432 771751936
pwrite 33554432 805306368
pwrite 33554432 838860800
pwrite 33554432 872415232
pwrite 33554432 905969664
pwrite 33554432 939524096
pwrite 33554432 973078528
pwrite 33554432 1006632960
pwrite 33554432 1040187392" ]; then
    echo "$0: output does not match expected"
    exit 1
fi
