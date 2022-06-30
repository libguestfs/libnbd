#!/usr/bin/env bash
# nbd client library in userspace
# Copyright (C) 2020-2022 Red Hat Inc.
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
requires nbdkit pattern --dump-plugin
requires nbdkit -U - null --run 'test "$uri" != ""'

output=dump-pattern.out
rm -f $output
cleanup_fn rm -f $output

nbdkit -U - pattern size=299 --run 'nbddump "$uri"' > $output

cat $output

if [ "$(cat $output)" != '0000000000: 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 08 |................|
0000000010: 00 00 00 00 00 00 00 10  00 00 00 00 00 00 00 18 |................|
0000000020: 00 00 00 00 00 00 00 20  00 00 00 00 00 00 00 28 |....... .......(|
0000000030: 00 00 00 00 00 00 00 30  00 00 00 00 00 00 00 38 |.......0.......8|
0000000040: 00 00 00 00 00 00 00 40  00 00 00 00 00 00 00 48 |.......@.......H|
0000000050: 00 00 00 00 00 00 00 50  00 00 00 00 00 00 00 58 |.......P.......X|
0000000060: 00 00 00 00 00 00 00 60  00 00 00 00 00 00 00 68 |.......`.......h|
0000000070: 00 00 00 00 00 00 00 70  00 00 00 00 00 00 00 78 |.......p.......x|
0000000080: 00 00 00 00 00 00 00 80  00 00 00 00 00 00 00 88 |................|
0000000090: 00 00 00 00 00 00 00 90  00 00 00 00 00 00 00 98 |................|
00000000a0: 00 00 00 00 00 00 00 a0  00 00 00 00 00 00 00 a8 |................|
00000000b0: 00 00 00 00 00 00 00 b0  00 00 00 00 00 00 00 b8 |................|
00000000c0: 00 00 00 00 00 00 00 c0  00 00 00 00 00 00 00 c8 |................|
00000000d0: 00 00 00 00 00 00 00 d0  00 00 00 00 00 00 00 d8 |................|
00000000e0: 00 00 00 00 00 00 00 e0  00 00 00 00 00 00 00 e8 |................|
00000000f0: 00 00 00 00 00 00 00 f0  00 00 00 00 00 00 00 f8 |................|
0000000100: 00 00 00 00 00 00 01 00  00 00 00 00 00 00 01 08 |................|
0000000110: 00 00 00 00 00 00 01 10  00 00 00 00 00 00 01 18 |................|
0000000120: 00 00 00 00 00 00 01 20  00 00 00                |....... ...     |' ]; then
    echo "$0: unexpected output from nbddump command"
    exit 1
fi
