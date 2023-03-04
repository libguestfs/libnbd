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

# This test requires nbdkit >= 1.22.
minor=$( nbdkit --dump-config | grep ^version_minor | cut -d= -f2 )
requires test $minor -ge 22

file=copy-nbd-to-hexdump.file
cleanup_fn rm -f $file

$VG nbdcopy -- [ nbdkit --exit-with-parent \
                        data data=' ( "Hello" )*2000 ' size=8192 \
               ] - | hexdump -C | tail > $file
cat $file

# Test the data is as expected.
test "$(cat $file)" = \
'00001f70  6c 6f 48 65 6c 6c 6f 48  65 6c 6c 6f 48 65 6c 6c  |loHelloHelloHell|
00001f80  6f 48 65 6c 6c 6f 48 65  6c 6c 6f 48 65 6c 6c 6f  |oHelloHelloHello|
00001f90  48 65 6c 6c 6f 48 65 6c  6c 6f 48 65 6c 6c 6f 48  |HelloHelloHelloH|
00001fa0  65 6c 6c 6f 48 65 6c 6c  6f 48 65 6c 6c 6f 48 65  |elloHelloHelloHe|
00001fb0  6c 6c 6f 48 65 6c 6c 6f  48 65 6c 6c 6f 48 65 6c  |lloHelloHelloHel|
00001fc0  6c 6f 48 65 6c 6c 6f 48  65 6c 6c 6f 48 65 6c 6c  |loHelloHelloHell|
00001fd0  6f 48 65 6c 6c 6f 48 65  6c 6c 6f 48 65 6c 6c 6f  |oHelloHelloHello|
00001fe0  48 65 6c 6c 6f 48 65 6c  6c 6f 48 65 6c 6c 6f 48  |HelloHelloHelloH|
00001ff0  65 6c 6c 6f 48 65 6c 6c  6f 48 65 6c 6c 6f 48 65  |elloHelloHelloHe|
00002000'
