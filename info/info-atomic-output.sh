#!/usr/bin/env bash
# nbd client library in userspace
# Copyright Red Hat
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

# Eval plugin was added in 1.18.
# --run $uri option was added in 1.14.
# So checking for the eval plugin is sufficient.
requires nbdkit eval --version

out=info-atomic-output.out
cleanup_fn rm -f $out

nbdkit -U - eval open='echo EIO fail >&2; exit 1' \
       --run '$VG nbdinfo --size "$uri"' > $out ||:
test ! -s $out
