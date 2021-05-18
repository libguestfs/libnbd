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

# Attempt to test using nbdinfo --map=qemu:dirty-bitmap.
# See also interop/dirty-bitmap.{c,sh}

. ../tests/functions.sh

set -e
set -x

requires qemu-img --version
requires qemu-io --version
requires qemu-nbd --version
requires bash -c 'qemu-nbd --help | grep pid-file'
requires_qemu
requires tr --version

# This test uses the qemu-nbd -B option.
if ! qemu-nbd --help | grep -sq -- -B; then
    echo "$0: skipping because qemu-nbd does not support the -B option"
    exit 77
fi

f=info-map-qemu-dirty-bitmap.qcow2
out=info-map-qemu-dirty-bitmap.out
cleanup_fn rm -f $f $out
rm -f $f $out

# Create file with intentionally different written areas vs. dirty areas
qemu-img create -f qcow2 $f 1M
qemu-io -f qcow2 -c 'w 0 64k' $f
cat <<'EOF' |
{'execute':'qmp_capabilities'}
{'execute':'blockdev-add','arguments':{'node-name':'n','driver':'qcow2','file':{'driver':'file','filename':'info-map-qemu-dirty-bitmap.qcow2'}}}
{'execute':'block-dirty-bitmap-add','arguments':{'node':'n','name':'bitmap0','persistent':true}}
{'execute':'quit'}
EOF
    $QEMU_BINARY -nodefaults -nographic -qmp stdio -machine none,accel=tcg
qemu-io -f qcow2 -c 'w 64k 64k' -c 'w -z 512k 64k' $f

# We have to run qemu-nbd and attempt to clean it up afterwards.
sock=$(mktemp -u /tmp/libnbd-test-info.XXXXXX)
pid=info-map-qemu-dirty-bitmap.pid
cleanup_fn rm -f $sock $pid
rm -f $sock $pid

qemu-nbd -t --socket=$sock --pid-file=$pid -f qcow2 -B bitmap0 $f &
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

$VG nbdinfo --map=qemu:dirty-bitmap:bitmap0 "nbd+unix://?socket=$sock" > $out
cat $out

if [ "$(tr -s ' ' < $out)" != " 0 65536 0 clean
 65536 65536 1 dirty
 131072 393216 0 clean
 524288 65536 1 dirty
 589824 458752 0 clean" ]; then
    echo "$0: unexpected output from nbdinfo --map"
    exit 1
fi
