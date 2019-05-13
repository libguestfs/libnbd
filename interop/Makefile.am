# nbd client library in userspace
# Copyright (C) 2013-2019 Red Hat Inc.
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

include $(top_srcdir)/subdir-rules.mk

TESTS_ENVIRONMENT = LIBNBD_DEBUG=1 $(top_builddir)/run
check_PROGRAMS =
TESTS =

if HAVE_NBD_SERVER

check_PROGRAMS += interop-nbd-server
TESTS += interop-nbd-server

interop_nbd_server_SOURCES = interop.c
interop_nbd_server_CPPFLAGS = \
	-I$(top_srcdir)/include \
	-DSERVER='"$(NBD_SERVER) -d -C /dev/null 0 %s", tmpfile' \
	-DEXPORT_NAME='""'
interop_nbd_server_CFLAGS = $(WARNINGS_CFLAGS)
interop_nbd_server_LDADD = $(top_builddir)/lib/libnbd.la

endif HAVE_NBD_SERVER

if HAVE_QEMU_NBD

check_PROGRAMS += interop-qemu-nbd
TESTS += interop-qemu-nbd

interop_qemu_nbd_SOURCES = interop.c
interop_qemu_nbd_CPPFLAGS = \
	-I$(top_srcdir)/include \
	-DSERVE_OVER_TCP=1 \
	-DSERVER=\"$(QEMU_NBD)\" \
	-DSERVER_PARAMS='"-f", "raw", "-x", "/", "-p", port_str, tmpfile' \
	-DEXPORT_NAME='"/"'
interop_qemu_nbd_CFLAGS = $(WARNINGS_CFLAGS)
interop_qemu_nbd_LDADD = $(top_builddir)/lib/libnbd.la

endif HAVE_QEMU_NBD
