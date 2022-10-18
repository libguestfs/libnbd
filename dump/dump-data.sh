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
requires nbdkit data --dump-plugin
requires nbdkit -U - null --run 'test "$uri" != ""'
requires nbdsh -c 'exit(not h.supports_uri())'

# This test requires nbdkit >= 1.22.
minor=$( nbdkit --dump-config | grep ^version_minor | cut -d= -f2 )
requires test $minor -ge 22

output=dump-data.out
rm -f $output
cleanup_fn rm -f $output

nbdkit -U - data data='
  @32768 1
  @65535 "hello, world!"
  @17825790 "spanning buffer boundary"
  @20000000 0
' --run 'nbddump "$uri"' > $output

cat $output

if [ "$(cat $output)" != '0000000000: 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00 |................|
*
0000008000: 01 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00 |................|
0000008010: 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00 |................|
*
000000fff0: 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 68 |...............h|
0000010000: 65 6c 6c 6f 2c 20 77 6f  72 6c 64 21 00 00 00 00 |ello, world!....|
0000010010: 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00 |................|
*
00010ffff0: 00 00 00 00 00 00 00 00  00 00 00 00 00 00 73 70 |..............sp|
0001100000: 61 6e 6e 69 6e 67 20 62  75 66 66 65 72 20 62 6f |anning buffer bo|
0001100010: 75 6e 64 61 72 79 00 00  00 00 00 00 00 00 00 00 |undary..........|
0001100020: 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00 |................|
*
0001312d00: 00                                               |.               |' ]; then
    echo "$0: unexpected output from nbddump command"
    exit 1
fi
