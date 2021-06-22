#!/usr/bin/env bash
# nbd client library in userspace
# Copyright (C) 2019-2021 Red Hat Inc.
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

# Test interop with qemu-storage-daemon.

source ../tests/functions.sh
set -e
set -x

requires test "x$QEMU_STORAGE_DAEMON" != "x"
requires sed --version
qsd_version="$($QEMU_STORAGE_DAEMON --version | \
               sed -n '1s/qemu-storage-daemon version \([0-9.]*\).*/\1/p')"
requires_not test "$qsd_version" = "5.1.0"
requires_not test "$qsd_version" = "5.2.0"
requires_not test "$qsd_version" = "6.0.0"
requires nbdsh --version
requires qemu-img --version

f="qemu-storage-daemon-disk.qcow2"
sock=$(mktemp -u /tmp/interop-qsd.XXXXXX)
rm -f $f $sock
cleanup_fn rm -f $f $sock

qemu-img create $f 10M -f qcow2

export f sock
$VG nbdsh -c - <<'EOF'
import os
import signal
import socket
import sys

# Get the name of q-s-d defined by ./configure.
qsd = os.environ["QEMU_STORAGE_DAEMON"]

# Test disk.
f = os.environ["f"]

# Unique socket name.
sock = os.environ["sock"]

# Create two sockets for client and server.  We pass the listening
# socket to q-s-d.
client_sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
qsd_sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
os.set_inheritable(qsd_sock.fileno(), True)
qsd_sock.bind(sock)
qsd_sock.listen()
client_sock.connect(sock)

# Assemble q-s-d command line.
argv = [
    qsd,
    "--blockdev",
    "qcow2,file.driver=file,file.filename=" + f + ",node-name=disk0",
    "--nbd-server", "addr.type=fd,addr.str=" + str(qsd_sock.fileno()),
    "--export", "nbd,node-name=disk0,id=nbd0,name=,writable=on"
]
print("server: %r" % argv, file=sys.stderr)

pid = os.fork()
if pid == 0:
    client_sock.close()
    os.execvp(qsd, argv)
else:
    qsd_sock.close()

# Connect to the server and test.
h.connect_socket(client_sock.fileno())

# Read and write.
buf = b"1" * 512
h.pwrite(buf, 512)
buf2 = h.pread(1024, 0)
assert bytearray(512) + buf == buf2

h.shutdown()

# Kill the server.
os.kill(pid, signal.SIGTERM)

EOF
