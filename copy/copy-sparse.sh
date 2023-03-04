#!/usr/bin/env bash
# nbd client library in userspace
# Copyright Red Hat
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

requires nbdkit --version
requires nbdkit --exit-with-parent --version
requires nbdkit data --version
requires nbdkit eval --version

out=copy-sparse.out
cleanup_fn rm -f $out

# Copy from a sparse data disk to an nbdkit-eval-plugin instance which
# is logging everything.  This allows us to see exactly what nbdcopy
# is writing, to ensure it is writing and zeroing the target as
# expected.  Force request size to match nbdkit default sparse
# allocator block size (32K).
$VG nbdcopy -S 0 --request-size=32768 -- \
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
LC_ALL=C sort -k1,1 -k2,2n -k3,3n -o $out $out

echo Output:
cat $out

# Check the output matches expected.
if [ "$(cat $out)" != "pwrite 1 4294967296
pwrite 32768 0
pwrite 32768 1073709056
pwrite 32768 4294934528
zero 134184960 32768 may_trim
zero 134184960 939524096 may_trim
zero 134184960 4160749568 may_trim
zero 134217728 134217728 may_trim
zero 134217728 268435456 may_trim
zero 134217728 402653184 may_trim
zero 134217728 536870912 may_trim
zero 134217728 671088640 may_trim
zero 134217728 805306368 may_trim
zero 134217728 1073741824 may_trim
zero 134217728 1207959552 may_trim
zero 134217728 1342177280 may_trim
zero 134217728 1476395008 may_trim
zero 134217728 1610612736 may_trim
zero 134217728 1744830464 may_trim
zero 134217728 1879048192 may_trim
zero 134217728 2013265920 may_trim
zero 134217728 2147483648 may_trim
zero 134217728 2281701376 may_trim
zero 134217728 2415919104 may_trim
zero 134217728 2550136832 may_trim
zero 134217728 2684354560 may_trim
zero 134217728 2818572288 may_trim
zero 134217728 2952790016 may_trim
zero 134217728 3087007744 may_trim
zero 134217728 3221225472 may_trim
zero 134217728 3355443200 may_trim
zero 134217728 3489660928 may_trim
zero 134217728 3623878656 may_trim
zero 134217728 3758096384 may_trim
zero 134217728 3892314112 may_trim
zero 134217728 4026531840 may_trim" ]; then
    echo "$0: output does not match expected"
    exit 1
fi
