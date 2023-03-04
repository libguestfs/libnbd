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

. ../tests/functions.sh

set -e
set -x

requires nbdkit --version
requires bash -c "nbdkit sh --dump-plugin | grep has_can_cache=1"

# --is read-only and --can write are tested in info-is-read-only.sh

# --is tls is tested in info-uri-nbds.sh and info-can-connect.sh

# --can connect is tested in info-can-connect.sh

# --can read is tested in info-can-read.sh

# --can zero is tested in info-can-zero.sh

# --can df is hard to test.  nbdkit newstyle probably always sets this
# and oldstyle never, but that feels like depending a bit too much on
# the implementation.

# --can structured-reply is not a per-export setting, but rather
# something set on the server as a whole.

nbdkit -v -U - sh - \
       --run '$VG nbdinfo --can structured-reply "nbd+unix:///?socket=$unixsocket"' <<'EOF'
case "$1" in
  get_size) echo 1024 ;;
  pread) ;;
  *) exit 2 ;;
esac
EOF

st=0
nbdkit -v -U - --no-sr sh - \
       --run '$VG nbdinfo --can structured-reply "nbd+unix:///?socket=$unixsocket"' <<'EOF' || st=$?
case "$1" in
  get_size) echo 1024 ;;
  pread) ;;
  *) exit 2 ;;
esac
EOF
test $st = 2

# --can cache and --can fua require special handling because in
# nbdkit-sh-plugin we must print "native" or "none".  Also the can_fua
# flag is only sent if the export is writable (hence can_write below).

for flag in cache fua; do
    export flag
    nbdkit -v -U - sh - \
           --run '$VG nbdinfo --can $flag "nbd+unix:///?socket=$unixsocket"' <<'EOF'
case "$1" in
  get_size) echo 1024 ;;
  pread) ;;
  can_write) ;;
  can_$flag) echo native ;;
  *) exit 2 ;;
esac
EOF

    st=0
    nbdkit -v -U - sh - \
           --run '$VG nbdinfo --can $flag "nbd+unix:///?socket=$unixsocket"' <<'EOF' || st=$?
case "$1" in
  get_size) echo 1024 ;;
  pread) ;;
  can_write) ;;
  can_$flag) echo none ;;
  *) exit 2 ;;
esac
EOF
    test $st = 2
done

# These ones are normal booleans.

for flag in fast_zero flush multi_conn trim ; do
    export flag
    nbdkit -v -U - sh - \
           --run '$VG nbdinfo --can $flag "nbd+unix:///?socket=$unixsocket"' <<'EOF'
case "$1" in
  get_size) echo 1024 ;;
  pread) ;;
  can_write) ;;
  can_$flag) exit 0 ;;
  *) exit 2 ;;
esac
EOF

    st=0
    nbdkit -v -U - sh - \
           --run '$VG nbdinfo --can $flag "nbd+unix:///?socket=$unixsocket"' <<'EOF' || st=$?
case "$1" in
  get_size) echo 1024 ;;
  pread) ;;
  can_write) ;;
  can_$flag) exit 3 ;;
  *) exit 2 ;;
esac
EOF
    test $st = 2
done
