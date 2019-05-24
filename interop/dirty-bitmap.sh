#!/usr/bin/env bash
# nbd client library in userspace
# Copyright (C) 2019 Red Hat Inc.
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

# Test reading qemu dirty-bitmap.

source ../tests/functions.sh
set -e
set -x

requires qemu-img --version
requires qemu-io --version
requires qemu-nbd --version
requires qemu-kvm --version

files="dirty-bitmap.sock dirty-bitmap.qcow2"
rm -f $files
cleanup_fn rm -f $files

qemu-img create -f qcow2 dirty-bitmap.qcow2 1M
cat <<'EOF' | qemu-kvm -nodefaults -nographic -qmp stdio
{'execute':'qmp_capabilities'}
{'execute':'blockdev-add','arguments':{'node-name':'n','driver':'qcow2','file':{'driver':'file','filename':'dirty-bitmap.qcow2'}}}
{'execute':'block-dirty-bitmap-add','arguments':{'node':'n','name':'bitmap0','persistent':true}}
{'execute':'quit'}
EOF
qemu-io -f qcow2 -c 'w 64k 64k' -c 'w 512k 64k' dirty-bitmap.qcow2

qemu-nbd -k $PWD/dirty-bitmap.sock -f qcow2 -B bitmap0 dirty-bitmap.qcow2 &
cleanup_fn kill $!

# No good way to wait for qemu-nbd to start server, so ...
sleep 5

# Run the test.
./dirty-bitmap dirty-bitmap.sock qemu:dirty-bitmap:bitmap0
