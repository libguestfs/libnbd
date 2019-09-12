#!/usr/bin/env bash
# nbd client library in userspace
# Copyright (C) 2019 Red Hat Inc.
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

# Test effects of nbdsh --base-allocation
fail=0

# Without --base-allocation, no meta context is requested
output=$(nbdkit -U - null --run 'nbdsh \
    --connect "nbd+unix://?socket=$unixsocket" \
    -c "print (h.can_meta_context (nbd.CONTEXT_BASE_ALLOCATION))"')
if test "x$output" != xFalse; then
    echo "$0: unexpected output: $output"
    fail=1
fi

# With --base-allocation (and a server that supports it), meta context works.
output=$(nbdkit -U - null --run 'nbdsh \
    --base-allocation --connect "nbd+unix://?socket=$unixsocket" \
    -c "print (h.can_meta_context (nbd.CONTEXT_BASE_ALLOCATION))"')
if test "x$output" != xTrue; then
    echo "$0: unexpected output: $output"
    fail=1
fi

# Again, but with --b after --connect, and with abbreviated option names
output=$(nbdkit -U - null --run 'nbdsh \
    --conn "nbd+unix://?socket=$unixsocket" --b \
    --command "print (h.can_meta_context (nbd.CONTEXT_BASE_ALLOCATION))"')
if test "x$output" != xTrue; then
    echo "$0: unexpected output: $output"
    fail=1
fi

if [[ $(nbdkit --help) =~ --no-sr ]]; then
    # meta context depends on server cooperation
    output=$(nbdkit -U - --no-sr null --run 'nbdsh \
      --connect "nbd+unix://?socket=$unixsocket" --base-allocation \
      -c "print (h.can_meta_context (nbd.CONTEXT_BASE_ALLOCATION))"')
    if test "x$output" != xFalse; then
        echo "$0: unexpected output: $output"
        fail=1
    fi
else
    echo "$0: nbdkit lacks --no-sr"
fi

exit $fail
