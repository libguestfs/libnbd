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

# This nbdkit plugin is used to test flags support in the libnbd
# library.  See tests/eflags.c

key=`cat $tmpdir/key`
print=`cat $tmpdir/print`
rc=`cat $tmpdir/rc`

if [ "$1" = "$key" ]; then
    if [ -n "$print" ]; then echo "$print"; fi
    exit "$rc"
fi

case "$1" in
    thread_model)
        echo parallel
    ;;
    config)
        case "$2" in
            key|print|rc) echo "$3" > $tmpdir/$2 ;;
            *)
                echo "unknown config key: $2" >&2
                exit 1
                ;;
        esac
        ;;
    get_size)
        echo 1M
        ;;
    pread)
        dd if=/dev/zero count=$3 iflag=count_bytes
        ;;
    can_write)
        # We have to answer this with true, otherwise the plugin will
        # be read-only and methods like can_trim will never be called.
        exit 0
        ;;
    can_zero)
	# We have to default to answering this with true before
	# can_fast_zero has an effect.
	exit 0
	;;
    *)
        exit 2
        ;;
esac
