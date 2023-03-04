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
requires nbdkit --exit-with-parent --version
requires nbdkit sparse-random --dump-plugin
requires qemu-img --version
#requires stat --version

# Check the compress driver is supported by this qemu-nbd.
# Note that qemu-nbd opens the socket before checking --image-opts!
sock=$(mktemp -u /tmp/libnbd-test-copy.XXXXXX)
cleanup_fn rm -f $sock
export sock
requires_not bash -c 'qemu-nbd -k $sock --image-opts driver=compress |&
                      grep -sq "Unknown driver.*compress"'

file1=copy-file-to-qcow2-compressed.file1
file2=copy-file-to-qcow2-compressed.file2
out1=copy-file-to-qcow2-compressed.out1
out2=copy-file-to-qcow2-compressed.out2
rm -f $file1 $file2 $out1 $out2
cleanup_fn rm -f $file1 $file2 $out1 $out2

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

# We used to do this but the test is not very reliable on some
# platforms.  Maybe due to type of filesystem or unpredictable content
# of sparse-random?
#
# file1 < file2 (shows the compression is having some effect).
#size1="$( stat -c %s $file1 )"
#size2="$( stat -c %s $file2 )"
#if [ $size1 -ge $size2 ]; then
#    echo "$0: qcow2 compression did not make the file smaller"
#    exit 1
#fi

# Check that there are some compressed clusters in the first file and
# no compressed clusters in the second file.  See:
# https://bugzilla.redhat.com/show_bug.cgi?id=2047660#c16

# Since we are comparing qemu-img strings.
export LC_ALL=C

qemu-img check $file1 > $out1
cat $out1
qemu-img check $file2 > $out2
cat $out2
! grep " 0.00% compressed clusters" $out1
grep " 0.00% compressed clusters" $out2

# Logical content of the files should be identical.
qemu-img compare -f qcow2 $file1 -F qcow2 $file2
