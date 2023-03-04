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

requires nbdkit --version
requires nbdkit file --version

# This test requires nbdkit >= 1.22.
minor=$( nbdkit --dump-config | grep ^version_minor | cut -d= -f2 )
requires test $minor -ge 22

out=info-list-uris.out
cleanup_fn rm -f $out

# nbdinfo --list is not very stable in the particular case where
# exports come and go while it is running.  This happens if we set the
# directory to be the current directory since other tests create
# temporary files here.  So point this to a more stable directory.

nbdkit -U - file dir=$srcdir/../examples \
       --run '$VG nbdinfo --list "$uri"' > $out
cat $out

# We expect to see URIs corresponding to some well-known files
# (ie. exports) in the examples directory.
grep "uri: nbd+unix:///LICENSE-FOR-EXAMPLES?socket=" $out
grep "uri: nbd+unix:///get-size.c?socket=" $out
