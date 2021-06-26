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

. ../tests/functions.sh

set -e
set -x

# nbdkit emulates zeroing so we have to use the nozero filter to test
# the negative case below.

requires nbdkit null --version
requires nbdkit null --filter=nozero --version

nbdkit -v -U - null \
       --run '$VG nbdinfo --can zero "nbd+unix:///?socket=$unixsocket"'

nbdkit -v -U - null \
       --filter=nozero \
       --run '! $VG nbdinfo --can zero "nbd+unix:///?socket=$unixsocket"'
