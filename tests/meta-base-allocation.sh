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

# This is used to test metadata context "base:allocation".
# See tests/meta-base-allocation.c and test 460 in language bindings.

case "$1" in
    thread_model)
        echo parallel
        ;;
    get_size)
        echo 65536
        ;;
    pread)
        dd if=/dev/zero count=$3 iflag=count_bytes
        ;;
    can_extents)
        exit 0
        ;;
    extents)
        echo     0  8192
        echo  8192  8192 hole
        echo 16384 16384 hole,zero
        echo 32768 16384 zero
        echo 49152 16384
        ;;
    *)
        exit 2
        ;;
esac
