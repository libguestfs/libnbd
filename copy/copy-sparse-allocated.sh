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

# Adapted from copy-sparse.sh.

. ../tests/functions.sh

set -e
set -x

requires nbdkit --version
requires nbdkit --exit-with-parent --version
requires nbdkit data --version
requires nbdkit eval --version

out=copy-sparse-allocated.out
cleanup_fn rm -f $out

$VG nbdcopy --allocated --request-size=32768 -- \
    [ nbdkit --exit-with-parent data data='
             1
             @1073741823 1
             @4294967295 1
             @4294967296 1
             ' ] \
    [ nbdkit --exit-with-parent eval \
             get_size=' echo 7E ' \
             pwrite=" echo \$@ >> $out " \
             trim=" echo \$@ >> $out " \
             zero=" echo \$@ >> $out " ]

sort -o $out $out

echo Output:
cat $out

if [ "$(cat $out)" != "pwrite 1 4294967296
pwrite 32768 0
pwrite 32768 1073709056
pwrite 32768 4294934528
zero 134184960 32768
zero 134184960 4160749568
zero 134184960 939524096
zero 134217728 1073741824
zero 134217728 1207959552
zero 134217728 134217728
zero 134217728 1342177280
zero 134217728 1476395008
zero 134217728 1610612736
zero 134217728 1744830464
zero 134217728 1879048192
zero 134217728 2013265920
zero 134217728 2147483648
zero 134217728 2281701376
zero 134217728 2415919104
zero 134217728 2550136832
zero 134217728 268435456
zero 134217728 2684354560
zero 134217728 2818572288
zero 134217728 2952790016
zero 134217728 3087007744
zero 134217728 3221225472
zero 134217728 3355443200
zero 134217728 3489660928
zero 134217728 3623878656
zero 134217728 3758096384
zero 134217728 3892314112
zero 134217728 402653184
zero 134217728 4026531840
zero 134217728 536870912
zero 134217728 671088640
zero 134217728 805306368" ]; then
    echo "$0: output does not match expected"
    exit 1
fi
