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

requires nbdkit --version
requires nbdkit memory --version

out=info-packets.out
cleanup_fn rm -f $out

nbdkit --no-sr -U - memory size=1M \
       --run '$VG nbdinfo "nbd+unix:///?socket=$unixsocket"' > $out
cat $out
grep "protocol: .*using simple packets" $out

nbdkit -U - memory size=1M \
       --run '$VG nbdinfo "nbd+unix:///?socket=$unixsocket"' > $out
cat $out
grep "protocol: .*using structured packets" $out
