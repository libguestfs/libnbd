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

# Test effects of nbdsh --base-allocation
fail=0

. ../tests/functions.sh
requires nbdsh -c 'exit(not h.supports_uri())'

# Without --base-allocation, no meta context is requested
output=$(nbdkit -U - null --run 'nbdsh \
    -u "nbd+unix://?socket=$unixsocket" \
    -c "print(h.can_meta_context(nbd.CONTEXT_BASE_ALLOCATION))"')
if test "x$output" != xFalse; then
    echo "$0: unexpected output: $output"
    fail=1
fi

# Can also use manual -c to request context before -u
output=$(nbdkit -U - null --run 'nbdsh \
-c "h.add_meta_context(nbd.CONTEXT_BASE_ALLOCATION)" \
-u "nbd+unix://?socket=$unixsocket" \
-c "print(h.can_meta_context(nbd.CONTEXT_BASE_ALLOCATION))"
')
if test "x$output" != xTrue; then
    echo "$0: unexpected output: $output"
    fail=1
fi

# With --base-allocation (and a server that supports it), meta context works.
output=$(nbdkit -U - null --run 'nbdsh \
    --base-allocation --uri "nbd+unix://?socket=$unixsocket" \
    --command "print(h.can_meta_context(nbd.CONTEXT_BASE_ALLOCATION))"')
if test "x$output" != xTrue; then
    echo "$0: unexpected output: $output"
    fail=1
fi

# Again, but with abbreviated option names
output=$(nbdkit -U - null --run 'nbdsh \
    --b -u "nbd+unix://?socket=$unixsocket" \
    -c "print(h.can_meta_context(nbd.CONTEXT_BASE_ALLOCATION))"')
if test "x$output" != xTrue; then
    echo "$0: unexpected output: $output"
    fail=1
fi

if [[ $(nbdkit --help) =~ --no-sr ]]; then
    # meta context depends on server cooperation
    output=$(nbdkit -U - --no-sr null --run 'nbdsh \
      --base-allocation -u "nbd+unix://?socket=$unixsocket" \
      -c "print(h.can_meta_context(nbd.CONTEXT_BASE_ALLOCATION))"')
    if test "x$output" != xFalse; then
        echo "$0: unexpected output: $output"
        fail=1
    fi
else
    echo "$0: nbdkit lacks --no-sr"
fi

# Test interaction with opt mode
output=$(nbdkit -U - null --run 'nbdsh \
    --opt-mode --base-allocation -u "nbd+unix://?socket=$unixsocket" \
    -c "
try:
    h.can_meta_context(nbd.CONTEXT_BASE_ALLOCATION)
    assert False
except nbd.Error:
    pass
" \
    -c "h.opt_go()" \
    -c "print(h.can_meta_context(nbd.CONTEXT_BASE_ALLOCATION))"')
if test "x$output" != xTrue; then
    echo "$0: unexpected output: $output"
    fail=1
fi

# And with --opt-mode, we can get away without --base-allocation
output=$(nbdkit -U - null --run 'nbdsh \
    --opt-mode -u "nbd+unix://?socket=$unixsocket" \
    -c "h.add_meta_context(nbd.CONTEXT_BASE_ALLOCATION)" \
    -c "h.opt_go()" \
    -c "print(h.can_meta_context(nbd.CONTEXT_BASE_ALLOCATION))"')
if test "x$output" != xTrue; then
    echo "$0: unexpected output: $output"
    fail=1
fi

exit $fail
