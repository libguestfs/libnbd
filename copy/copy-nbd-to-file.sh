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

requires nbdkit --exit-with-parent --version
requires hexdump -C /dev/null
requires stat --version

file=copy-nbd-to-file.file
cleanup_fn rm -f $file

$VG nbdcopy -- [ nbdkit --exit-with-parent -v data data='0x55 0xAA @268435454 0xAA 0x55' ] $file
if [ "$(stat -c %s $file)" -ne $(( 256 * 1024 * 1024 )) ]; then
    echo "$0: incorrect amount of data copied"
    exit 1
fi

# Test the data is as expected.
test "$(hexdump -C $file)" = \
'00000000  55 aa 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |U...............|
00000010  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
*
0ffffff0  00 00 00 00 00 00 00 00  00 00 00 00 00 00 aa 55  |...............U|
10000000'
