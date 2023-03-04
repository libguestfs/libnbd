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
requires nbdkit -U - null --run 'test "$uri" != ""'
requires nbdkit sh --version
requires tr --version

out=info-base-allocation-weird.out
cleanup_fn rm -f $out
rm -f $out

# This is a "weird" server that returns extents that are all 1 byte.
nbdkit -U - sh - \
       --run '$VG nbdinfo --map "$uri"' > $out <<'EOF'
case "$1" in
  get_size) echo 32 ;;
  pread) dd if=/dev/zero count=$3 iflag=count_bytes ;;
  can_extents) exit 0 ;;
  extents) echo $4 1 `[ $4 -ge 16 ] && [ $4 -le 19 ]; echo $?`;;
  *) exit 2 ;;
esac
EOF

cat $out

if [ "$(tr -s ' ' < $out)" != " 0 16 1 hole
 16 4 0 data
 20 12 1 hole" ]; then
    echo "$0: unexpected output from nbdinfo --map"
    exit 1
fi
