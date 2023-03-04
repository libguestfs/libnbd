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

requires $QEMU_NBD --pid-file=test.pid --version
requires truncate --version
requires jq --version

img=info-list-json-qemu.img
out=info-list-json-qemu.out
pid=info-list-json-qemu.pid
sock=$(mktemp -u /tmp/libnbd-test-info.XXXXXX)
cleanup_fn rm -f $img $out $pid $sock
rm -f $img $out $pid $sock

truncate -s 1M $img
$QEMU_NBD -t --socket=$sock --pid-file=$pid -x "hello" -D "world" $img &
cleanup_fn kill $!

wait_for_pidfile qemu-nbd $pid

# Test twice, once with an export name not on the list,...
$VG nbdinfo "nbd+unix://?socket=$sock" --list --json > $out
jq . < $out

grep '"export-name": "hello"' $out
grep '"description": "world"' $out
grep '"export-size": 1048576' $out

# ...and again with the export name included
$VG nbdinfo "nbd+unix:///hello?socket=$sock" --list --json > $out
jq . < $out

grep '"export-name": "hello"' $out
grep '"description": "world"' $out
grep '"export-size": 1048576' $out
