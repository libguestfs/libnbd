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

# This is used to test nbd_opt_info in various language bindings.
# See tests/opt-info.c and test 230 in language bindings.

# Export "a" is unavailable, other exports reflect their name as content.
# Export "b" is writeable, others are read-only.
# Thus, size and read-only status can be used to check info/go requests.

case "$1" in
    open)
        if test "$3" = a; then
            echo ENOENT export not available >&2
            exit 1
        fi
        echo "$3" ;;
    get_size)
        printf %s "$2" | wc -c ;;
    can_write)
        if test "$2" != b; then
            exit 3;
        fi ;;
    pread)
        printf %s "$2" | dd bs=1 count=$3 skip=$4 ;;
    pwrite)
        dd of=/dev/null ;;
    *)
        exit 2 ;;
esac
