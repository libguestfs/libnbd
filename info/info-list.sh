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

requires nbdkit --filter=exportname memory --version

out=info-list.out
cleanup_fn rm -f $out
rm -f $out

# Test twice, once with an export name not on the list,...
nbdkit -U - -e nosuch --filter=exportname memory 1M \
       exportname=hello exportname=goodbye \
       exportname-strict=true exportname-list=explicit exportdesc=fixed:world \
       --run '$VG nbdinfo --list "$uri"' > $out
cat $out

grep 'export="hello":' $out
grep 'description: world' $out
grep 'export-size: 1048576' $out

# ...and again with the export name included
nbdkit -U - -e hello --filter=exportname memory 1M \
       exportname=hello exportname=goodbye \
       exportname-strict=true exportname-list=explicit exportdesc=fixed:world \
       --run '$VG nbdinfo --list "$uri"' > $out
cat $out

grep 'export="hello":' $out
grep 'description: world' $out
grep 'export-size: 1048576' $out
