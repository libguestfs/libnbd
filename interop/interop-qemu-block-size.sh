#!/usr/bin/env bash
# nbd client library in userspace
# Copyright (C) 2019-2022 Red Hat Inc.
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

# Test interop with qemu-nbd block sizes.

source ../tests/functions.sh
set -e
set -x

# versions of qemu-nbd older than 4.0 reported inaccurate block sizes
# Use 'qemu-nbd --list' as our witness, it was also added in 4.0
requires qemu-nbd --list --version
requires nbdsh --version
requires qemu-img --version
requires truncate --version
requires timeout --version

f="qemu-block-size.raw"
sock=$(mktemp -u /tmp/interop-qemu.XXXXXX)
rm -f $f $sock
cleanup_fn rm -f $f $sock

truncate --size=10M $f
export sock
fail=0

# run_test FMT REQUEST EXP_INFO EXP_GO
run_test() {
    rm -f $sock
    # No -t or -e, so qemu-nbd should exit once nbdsh disconnects
    timeout 60s qemu-nbd -k $sock $1 $f &
    pid=$!
    # Wait for the socket to appear
    for i in {1..30}; do
        if test -e $sock; then
            break
        fi
        sleep 1
    done
    $VG nbdsh -c - <<EOF
import os

sock = os.environ["sock"]

h.set_opt_mode(True)
assert h.get_request_block_size()
h.set_request_block_size($2)
assert h.get_request_block_size() is $2
h.connect_unix(sock)

if not h.aio_is_negotiating():
    h.shutdown()
    exit(77)    # Oldstyle negotiation lacks block size advertisement

try:
    h.opt_info()
    assert h.get_block_size(nbd.SIZE_MINIMUM) == $3
except nbd.Error:
    assert $3 == 0

h.opt_go()
assert h.get_block_size(nbd.SIZE_MINIMUM) == $4

h.shutdown()
EOF
    st=$?
    if [ $st = 77 ]; then
        echo "$0: skipping: qemu-nbd too old for this test"
        exit 77
    elif [ $st != 0 ]; then
        fail=1
    fi
    wait $pid || fail=1
}

# Without '-f raw', qemu-nbd forces sector granularity to prevent writing
# to sector 0 from changing the disk type.  However, if the client does
# not request block sizes, it reports a size then errors out for NBD_OPT_INFO,
# while fudging size for NBD_OPT_GO.
run_test '' True 512 512
run_test '' False 0 1

# With '-f raw', qemu-nbd always exposes byte-level granularity for files.
run_test '-f raw' True 1 1
run_test '-f raw' False 1 1

exit $fail
