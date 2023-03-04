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

requires $QEMU_NBD --version
requires qemu-img --version

file=dump-empty-qcow2.qcow2
output=dump-empty-qcow2.out
rm -f $file $output
cleanup_fn rm -f $file $output

size=1G

# Create a large, empty qcow2 file.
qemu-img create -f qcow2 $file $size

# Dump it and check the output.
nbddump -- [ $QEMU_NBD -r -f qcow2 $file ] > $output
cat $output

if [ "$(cat $output)" != '0000000000: 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00 |................|
*
003ffffff0: 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00 |................|' ]; then
    echo "$0: unexpected output from nbddump command"
    exit 1
fi
