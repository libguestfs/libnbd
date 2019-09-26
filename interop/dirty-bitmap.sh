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

# This test uses the qemu-nbd -B option.
if ! qemu-nbd --help | grep -sq -- -B; then
    echo "$0: skipping because qemu-nbd does not support the -B option"
    exit 77
fi

files="dirty-bitmap.sock dirty-bitmap.qcow2"
rm -f $files
cleanup_fn rm -f $files

# Create file with intentionally different written areas vs. dirty areas
qemu-img create -f qcow2 dirty-bitmap.qcow2 1M
qemu-io -f qcow2 -c 'w 0 64k' dirty-bitmap.qcow2
cat <<'EOF' | qemu-kvm -nodefaults -nographic -qmp stdio
{'execute':'qmp_capabilities'}
{'execute':'blockdev-add','arguments':{'node-name':'n','driver':'qcow2','file':{'driver':'file','filename':'dirty-bitmap.qcow2'}}}
{'execute':'block-dirty-bitmap-add','arguments':{'node':'n','name':'bitmap0','persistent':true}}
{'execute':'quit'}
EOF
qemu-io -f qcow2 -c 'w 64k 64k' -c 'w -z 512k 64k' dirty-bitmap.qcow2

qemu-nbd -k $PWD/dirty-bitmap.sock -f qcow2 -B bitmap0 dirty-bitmap.qcow2 &
qemu_pid=$!
cleanup_fn kill $qemu_pid

# No good way to wait for qemu-nbd to start server, so ...
for ((i = 0; i < 300; i++)); do
  if [ -r $PWD/dirty-bitmap.sock ]; then
    break
  fi
  kill -s 0 $qemu_pid 2>/dev/null
  if test $? != 0; then
    echo "qemu-nbd unexpectedly quit" 2>&1
    exit 1
  fi
  sleep 0.1
done

# Run the test.
$VG ./dirty-bitmap dirty-bitmap.sock qemu:dirty-bitmap:bitmap0
