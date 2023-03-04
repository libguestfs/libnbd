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

# This is used to test nbd_opt_list in various language bindings.
# See tests/opt-list.c and test 220 in language bindings.

case "$1" in
    config)
        if [ "$2" != mode ]; then echo EINVAL >&2; exit 1; fi
        echo "$3" > "$tmpdir/mode" ;;
    list_exports)
        read i < "$tmpdir/mode"
        case $i in
            0) echo EINVAL listing not supported >&2; exit 1 ;;
            1) echo NAMES; echo a; echo b ;;
            2) echo NAMES ;;
            *) echo NAMES; echo a ;;
        esac ;;
    get_size)
        echo 512 ;;
    pread)
        dd bs=1 if=/dev/zero count=$3 ;;
    *)
        exit 2 ;;
esac
