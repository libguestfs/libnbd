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

# Stream from a sparse NBD source.  The NBD source uses a bandwidth
# limiter so this will be very slow unless nbdcopy correctly handles
# extents.

. ../tests/functions.sh

set -e
set -x

requires nbdkit --version
requires nbdkit --exit-with-parent --version
requires nbdkit data --version
requires nbdkit --version --filter=rate null

# Copy rate is 512Kbps.  nbdcopy should only need to copy two 32Kbyte
# extents (2 * 32 * 8 = 512) so this should not take longer than a
# second.  But if nbdcopy is ignoring extents it will take much, much
# longer.
$VG nbdcopy -- \
    [ nbdkit --exit-with-parent \
             data data=' 1  @1073741823 1 ' \
             --filter=rate rate=512K \
    ] \
    - >/dev/null
