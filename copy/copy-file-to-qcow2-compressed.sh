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

requires $QEMU_NBD --version
requires nbdkit --exit-with-parent --version
requires nbdkit sparse-random --dump-plugin
requires qemu-img --version
requires stat --version

file1=copy-file-to-qcow2-compressed.file1
file2=copy-file-to-qcow2-compressed.file2
rm -f $file1 $file2
cleanup_fn rm -f $file1 $file2

size=1G
seed=$RANDOM

# Create a compressed qcow2 file1.
#
# sparse-random files should compress easily because by default each
# block uses repeated bytes.
qemu-img create -f qcow2 $file1 $size
opts=driver=compress
opts+=,file.driver=qcow2
opts+=,file.file.driver=file
opts+=,file.file.filename=$file1
nbdcopy -- [ nbdkit --exit-with-parent sparse-random $size seed=$seed ] \
        [ $QEMU_NBD --image-opts "$opts" ]

ls -l $file1

# Create an uncompressed qcow2 file2 with the same data.
qemu-img create -f qcow2 $file2 $size
opts=driver=qcow2
opts+=,file.driver=file
opts+=,file.filename=$file2
nbdcopy -- [ nbdkit --exit-with-parent sparse-random $size seed=$seed ] \
        [ $QEMU_NBD --image-opts "$opts" ]

ls -l $file2

# file1 < file2 (shows the compression is having some effect).
size1="$( stat -c %s $file1 )"
size2="$( stat -c %s $file2 )"
if [ $size1 -ge $size2 ]; then
    echo "$0: qcow2 compression did not make the file smaller"
    exit 1
fi

# Logical content of the files should be identical.
qemu-img compare -f qcow2 $file1 -F qcow2 $file2
