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

# Test nbds:// URIs on the command line and in output.

. ../tests/functions.sh

set -e
set -x

requires nbdkit --version
requires nbdkit null --version
requires jq --version

# Requires that the test certificates were created.
pki=../tests/pki
requires test -f $pki/stamp-pki

d=info-uri-nbds.d
out=info-uri-nbds.out
rm -f $out
rm -rf $d
cleanup_fn rm -f $out
cleanup_fn rm -rf $d

# Run nbdkit with TLS.
#
# nbdkit does not add tls-certificates to the $uri it generates
# (because there's not really a good way to know if the resulting URI
# would be valid) so we need to construct a URI here.
export pki
nbdkit -U - --tls=require --tls-verify-peer --tls-certificates=$pki \
       null size=1M \
       --run '$VG nbdinfo --json "nbds+unix:///?socket=$unixsocket&tls-certificates=$pki"' > $out
cat $out
jq . < $out

[[ $( jq -r '.exports[0] | .uri' < $out ) =~ \
   ^nbds\+unix:///\?socket=.*\&tls-certificates=$pki ]]
