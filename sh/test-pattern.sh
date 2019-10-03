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

# Test interaction with nbdkit, and for correct global handling over -c.

. ../tests/functions.sh
requires nbdkit --exit-with-parent --version
requires nbdsh -c 'exit(not h.supports_uri())'

sock=`mktemp -u /tmp/nbdsh.XXXXXX`
pidfile=test-dump.pid
cleanup_fn rm -f $sock $pidfile
nbdkit -v -P $pidfile --exit-with-parent -U $sock pattern size=1m &

# Wait for the pidfile to appear.
for i in {1..60}; do
    if test -f "$pidfile"; then
	break
    fi
    sleep 1
done
if ! test -f "$pidfile"; then
    echo "$0: nbdkit PID file $pidfile was not created"
    exit 1
fi

nbdsh -u "nbd+unix://?socket=$sock" \
    -c '
def size():
  return h.get_size()
' \
    -c 'assert 1024*1024 == size()' \
    -c 'assert h.pread(8, 8) == b"\x00\x00\x00\x00\x00\x00\x00\x08"'
