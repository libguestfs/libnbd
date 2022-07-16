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

# Test nbd+vsock:// URIs

source ./functions.sh
set -e
set -x

# vsock loopback support was only added in Linux kernel 5.6 and
# requires a particular kernel module to be loaded.
requires_linux_kernel_version 5.6
requires_vsock_support

# This test requires nbdkit >= 1.16 which added the --vsock option.
requires nbdkit --version
minor=$( nbdkit --dump-config | grep ^version_minor | cut -d= -f2 )
requires test $minor -ge 16

requires nbdkit pattern --version

# Check we built nbdinfo (this implicitly checks we have libxml2).
requires nbdinfo --version

# Because vsock ports are 32 bits, we can basically pick one at random
# and be sure that it's not used.  However we must pick one >= 1024
# because the ports below this are privileged.
#port=$(( 1024 + $RANDOM + ($RANDOM << 16) ))
#
# We would do that, but libxml2 is broken, see:
# https://mail.gnome.org/archives/xml/2020-October/msg00001.html
# https://mail.gnome.org/archives/xml/2020-October/msg00002.html
port=$(( 1024 + $RANDOM + ($RANDOM << 11) ))

# Form the NBD URI.  Don't rely on nbdkit $uri support since it didn't
# always support vsock, and anyway we want to test libnbd here.
# 1 == VMADDR_CID_LOCAL
export our_uri="nbd+vsock://1:$port"

nbdkit --vsock --port $port \
       pattern size=8M \
       --run 'nbdinfo "$our_uri"'
