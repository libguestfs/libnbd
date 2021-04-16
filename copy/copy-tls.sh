#!/usr/bin/env bash
# nbd client library in userspace
# Copyright (C) 2020-2021 Red Hat Inc.
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

# Check that TLS to/from NBD servers works.  It should be transparent
# as far as nbdcopy code is concerned, but using nbdcopy and TLS
# together stresses the library in unusual ways.

. ../tests/functions.sh

set -e
set -x

requires test "x$PSKTOOL" != "x"
requires nbdkit --exit-with-parent --version
requires bash -c "nbdkit --dump-config | grep tls=yes"
requires cmp /dev/null /dev/null
requires hexdump --version

pskfile=copy-tls.psk
pidfile1=copy-tls.pid1
pidfile2=copy-tls.pid2
file1=copy-tls.file1
file2=copy-tls.file2
sock1=$(mktemp -u /tmp/libnbd-test-copy.XXXXXX)
sock2=$(mktemp -u /tmp/libnbd-test-copy.XXXXXX)
cleanup_fn rm -f $pskfile $pidfile1 $pidfile2 $file1 $file2 $sock1 $sock2

$PSKTOOL -u alice -p $pskfile

nbdkit --exit-with-parent -f -v -P $pidfile1 -U $sock1 \
       --tls=require --tls-psk=$pskfile \
       pattern size=100M &
uri1="nbds+unix://alice@/?socket=$sock1&tls-psk-file=$pskfile"
# Wait for the pidfile to appear.
for i in {1..60}; do
    if test -f $pidfile1; then
        break
    fi
    sleep 1
done
if ! test -f $pidfile1; then
    echo "$0: nbdkit did not start up"
    exit 1
fi

nbdkit --exit-with-parent -f -v -P $pidfile2 -U $sock2 \
       --tls=require --tls-psk=$pskfile \
       memory size=100M &
uri2="nbds+unix://alice@/?socket=$sock2&tls-psk-file=$pskfile"
# Wait for the pidfile to appear.
for i in {1..60}; do
    if test -f $pidfile2; then
        break
    fi
    sleep 1
done
if ! test -f $pidfile2; then
    echo "$0: nbdkit did not start up"
    exit 1
fi

$VG nbdcopy "$uri1" "$uri2"

# Download the file from both servers and check they are the same.
$VG nbdcopy "$uri1" $file1
$VG nbdcopy "$uri2" $file2

ls -l $file1 $file2
cmp $file1 $file2

# Test the data is at least non-zero.
test "$(hexdump -C $file1 | head -1)" = "00000000  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 08  |................|"
