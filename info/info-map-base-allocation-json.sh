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
requires jq --version

out=info-base-allocation-json.out
cleanup_fn rm -f $out
rm -f $out

# Note the memory plugin uses a 32K page size, and extents
# are always aligned with this.
nbdkit -U - memory 1M --run '
    nbdsh -u "$uri" \
          -c "h.pwrite(b\"\\x01\"*131072, 0)" \
          -c "h.pwrite(b\"\\x02\"*131072, 320*1024)" &&
    $VG nbdinfo --map --json "$uri"
' > $out

cat $out
jq < $out

test $( jq -r '.[0].offset' < $out ) -eq 0
test $( jq -r '.[0].length' < $out ) -eq 131072
test $( jq -r '.[0].type' < $out ) -eq 0
test $( jq -r '.[0].description' < $out ) = "allocated"

test $( jq -r '.[3].offset' < $out ) -eq 458752
test $( jq -r '.[3].length' < $out ) -eq 589824
test $( jq -r '.[3].type' < $out ) -eq 3
test $( jq -r '.[3].description' < $out ) = "hole,zero"
