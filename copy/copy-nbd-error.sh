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

# Tests several scenarios of handling NBD server errors
# Serves as a regression test for the CVE-2022-0485 fix.

. ../tests/functions.sh

set -e
set -x

requires nbdkit --exit-with-parent --version
requires nbdkit --filter=noextents null --version
requires nbdkit --filter=error pattern --version
requires nbdkit --filter=nozero memory --version

fail=0

# Failure to get block status should not be fatal, but merely downgrade to
# reading the entire image as if data
echo "Testing extents failures on source"
$VG nbdcopy -- [ nbdkit --exit-with-parent -v --filter=error pattern 5M \
    error-extents-rate=1 ] null: || fail=1

# Failure to read should be fatal
echo "Testing read failures on non-sparse source"
$VG nbdcopy -- [ nbdkit --exit-with-parent -v --filter=error pattern 5M \
    error-pread-rate=0.5 ] null: && fail=1

# However, reliable block status on a sparse image can avoid the need to read
echo "Testing read failures on sparse source"
$VG nbdcopy -- [ nbdkit --exit-with-parent -v --filter=error null 5M \
    error-pread-rate=1 ] null: || fail=1

# Failure to write data should be fatal
echo "Testing write data failures on arbitrary destination"
$VG nbdcopy -- [ nbdkit --exit-with-parent -v pattern 5M ] \
    [ nbdkit --exit-with-parent -v --filter=error --filter=noextents \
        memory 5M error-pwrite-rate=0.5 ] && fail=1

# However, writing zeroes can bypass the need for normal writes
echo "Testing write data failures from sparse source"
$VG nbdcopy -- [ nbdkit --exit-with-parent -v null 5M ] \
    [ nbdkit --exit-with-parent -v --filter=error --filter=noextents \
        memory 5M error-pwrite-rate=1 ] || fail=1

# Failure to write zeroes should be fatal
echo "Testing write zero failures on arbitrary destination"
$VG nbdcopy -- [ nbdkit --exit-with-parent -v null 5M ] \
    [ nbdkit --exit-with-parent -v --filter=error memory 5M \
        error-zero-rate=1 ] && fail=1

# However, assuming/learning destination is zero can skip need to write
echo "Testing write failures on pre-zeroed destination"
$VG nbdcopy --destination-is-zero -- \
    [ nbdkit --exit-with-parent -v null 5M ] \
    [ nbdkit --exit-with-parent -v --filter=error memory 5M \
        error-pwrite-rate=1 error-zero-rate=1 ] || fail=1

# Likewise, when write zero is not advertised, fallback to normal write works
echo "Testing write zeroes to destination without zero support"
$VG nbdcopy -- [ nbdkit --exit-with-parent -v null 5M ] \
    [ nbdkit --exit-with-parent -v --filter=nozero --filter=error memory 5M \
        error-zero-rate=1 ] || fail=1

exit $fail
