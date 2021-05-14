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
requires jq --version

out=info-base-allocation-json.out
cleanup_fn rm -f $out
rm -f $out

# The sparse allocator used by nbdkit-data-plugin uses a 32K page
# size, and extents are always aligned with this.
nbdkit -U - data data='1 @131072 2' size=1M \
       --run '$VG nbdinfo --map --json "$uri"' > $out

cat $out
jq . < $out

test $( jq -r '.[0].offset' < $out ) -eq 0
test $( jq -r '.[0].length' < $out ) -eq 32768
test $( jq -r '.[0].type' < $out ) -eq 0
test $( jq -r '.[0].description' < $out ) = "allocated"

test $( jq -r '.[3].offset' < $out ) -eq 163840
test $( jq -r '.[3].length' < $out ) -eq 884736
test $( jq -r '.[3].type' < $out ) -eq 3
test $( jq -r '.[3].description' < $out ) = "hole,zero"
