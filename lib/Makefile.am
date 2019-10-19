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

generator_built = \
	api.c \
	states.c \
	states-run.c \
	states.h \
	unlocked.h \
	$(NULL)

EXTRA_DIST = \
	$(generator_built) \
	libnbd.syms \
	$(NULL)

lib_LTLIBRARIES = libnbd.la

BUILT_SOURCES = $(generator_built)

libnbd_la_SOURCES = \
	aio.c \
	api.c \
	connect.c \
	crypto.c \
	debug.c \
	disconnect.c \
	errors.c \
	flags.c \
	handle.c \
	internal.h \
	is-state.c \
	nbd-protocol.h \
	poll.c \
	protocol.c \
	rw.c \
	socket.c \
	states.c \
	states-run.c \
	states.h \
	unlocked.h \
	uri.c \
	utils.c \
	$(NULL)
libnbd_la_CPPFLAGS = \
	-I$(top_srcdir)/include \
	-I$(top_srcdir)/common/include \
	-Dsysconfdir=\"$(sysconfdir)\" \
	$(NULL)
libnbd_la_CFLAGS = \
	$(WARNINGS_CFLAGS) \
	$(PTHREAD_CFLAGS) \
	$(GNUTLS_CFLAGS) \
	$(LIBXML2_CFLAGS) \
	$(NULL)
libnbd_la_LIBADD = \
	$(GNUTLS_LIBS) \
	$(LIBXML2_LIBS) \
	$(NULL)
libnbd_la_LDFLAGS = \
	$(PTHREAD_LIBS) \
	-Wl,--version-script=$(srcdir)/libnbd.syms \
	-version-info 0:0:0 \
	$(NULL)

# pkg-config / pkgconf

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libnbd.pc
