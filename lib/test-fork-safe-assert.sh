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

set +e

./test-fork-safe-assert 2>test-fork-safe-assert.err
exit_status=$?

set -e

test $exit_status -gt 128
signal_name=$(kill -l $exit_status)
test "x$signal_name" = xABRT || test "x$signal_name" = xSIGABRT

ptrn="^test-fork-safe-assert\\.c:[0-9]+: main: Assertion \`FALSE' failed\\.\$"
grep -E -q -- "$ptrn" test-fork-safe-assert.err
grep -v -q "\`TRUE'" test-fork-safe-assert.err
