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
requires tr --version

out=info-map-totals.out
cleanup_fn rm -f $out
rm -f $out

# The sparse allocator used by nbdkit-data-plugin uses a 32K page
# size, and extents are always aligned with this.
nbdkit -U - data data='1 @131072 2' size=1M \
       --run '$VG nbdinfo --map --totals "$uri"' > $out

cat $out

if [ "$(tr -s ' ' < $out)" != " 65536 6.2% 0 data
 983040 93.8% 3 hole,zero" ]; then
    echo "$0: unexpected output from nbdinfo --map"
    exit 1
fi
