#!/usr/bin/env bash
# nbd client library in userspace
# Copyright (C) 2019-2020 Red Hat Inc.
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

# Test how nbdsh handles errors
. ../tests/functions.sh

set -xe
fail=0

out=test-error.out
err=test-error.err
files="$out $err"
cleanup_fn rm -f $files
rm -f $files

# Test behavior with unknown option.  No python trace should be present.
nbdsh --no-such >$out 2>$err && fail=1
test ! -s $out
cat $err
grep Traceback $err && fail=1
grep '^nbdsh: .*unrecognized.*no-such' $err

# Test behavior when -u fails.  No python trace should be present.
nbdsh -u 'nbd+unix:///?socket=/nosuchsock' >$out 2>$err && fail=1
test ! -s $out
cat $err
grep Traceback $err && fail=1
grep '^nbdsh: unable to connect to uri.*nosuchsock' test-error.err

exit $fail
