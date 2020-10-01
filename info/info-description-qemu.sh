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

requires qemu-nbd --version
requires bash -c 'qemu-nbd --help | grep pid-file'
requires truncate --version

img=info-description-qemu.img
out=info-description-qemu.out
pid=info-description-qemu.pid
sock=$(mktemp -u /tmp/libnbd-test-info.XXXXXX)
cleanup_fn rm -f $img $out $pid $sock
rm -f $img $out $pid $sock

truncate -s 1M $img
qemu-nbd -f raw -t --socket=$sock --pid-file=$pid -x "hello" -D "world" $img &
cleanup_fn kill $!

# Wait for qemu-nbd to start up.
for i in {1..60}; do
    if test -f $pid; then
        break
    fi
    sleep 1
done
if ! test -f $pid; then
    echo "$0: qemu-nbd did not start up"
    exit 1
fi

$VG nbdinfo "nbd+unix:///hello?socket=$sock" > $out
cat $out

grep 'export="hello":' $out
grep 'description: world' $out
grep 'export-size: 1048576' $out
