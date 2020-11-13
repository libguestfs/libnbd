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

# This test depends on the nbdkit default sparse block size (32K).

. ../tests/functions.sh

set -e
set -x

requires nbdkit --version
requires nbdkit --exit-with-parent --version
requires nbdkit data --version
requires nbdkit eval --version

out=copy-sparse.out
cleanup_fn rm -f $out

# Copy from a sparse data disk to an nbdkit-eval-plugin instance which
# is logging everything.  This allows us to see exactly what nbdcopy
# is writing, to ensure it is writing and trimming the target as
# expected.
$VG nbdcopy -- \
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

# Order of the output could vary because requests are sent in
# parallel.
sort -n -o $out $out

echo Output:
cat $out

# Check the output matches expected.
if [ "$(cat $out)" != "pwrite 1 4294967296
pwrite 32768 0
pwrite 32768 1073709056
pwrite 32768 4294934528
trim 134184960 32768
trim 134184960 4160749568
trim 134184960 939524096
trim 134217728 1073741824
trim 134217728 1207959552
trim 134217728 134217728
trim 134217728 1342177280
trim 134217728 1476395008
trim 134217728 1610612736
trim 134217728 1744830464
trim 134217728 1879048192
trim 134217728 2013265920
trim 134217728 2147483648
trim 134217728 2281701376
trim 134217728 2415919104
trim 134217728 2550136832
trim 134217728 268435456
trim 134217728 2684354560
trim 134217728 2818572288
trim 134217728 2952790016
trim 134217728 3087007744
trim 134217728 3221225472
trim 134217728 3355443200
trim 134217728 3489660928
trim 134217728 3623878656
trim 134217728 3758096384
trim 134217728 3892314112
trim 134217728 402653184
trim 134217728 4026531840
trim 134217728 536870912
trim 134217728 671088640
trim 134217728 805306368" ]; then
    echo "$0: output does not match expected"
    exit 1
fi
