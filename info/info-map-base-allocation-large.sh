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

. ../tests/functions.sh

set -e
set -x

requires nbdkit --version
requires nbdsh --version
requires tr --version

out=info-base-allocation.out
cleanup_fn rm -f $out
rm -f $out

# Note the memory plugin uses a 32K page size, and extents
# are always aligned with this.
nbdkit -U - memory 6G --run '
    nbdsh -u "$uri" \
          -c "h.pwrite(b\"\\x01\"*131072, 0)" \
          -c "h.pwrite(b\"\\x02\"*131072, 320*1024)" &&
    $VG nbdinfo --map "$uri"
' > $out

cat $out

if [ "$(tr -s ' ' < $out)" != " 0 131072 0 allocated
 131072 196608 3 hole,zero
 327680 131072 0 allocated
 458752 4294508544 3 hole,zero
4294967296 2147483648 3 hole,zero" ]; then
    echo "$0: unexpected output from nbdinfo --map"
    exit 1
fi
