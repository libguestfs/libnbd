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

CLEANFILES = *~

BUILT_SOURCES = \
	api.c \
	states.c \
	states.h \
	unlocked.h

EXTRA_DIST = \
	$(BUILT_SOURCES)

lib_LTLIBRARIES = libnbd.la

libnbd_la_SOURCES = \
	aio.c \
	api.c \
	create.c \
	connect.c \
	internal.h \
	nbd-protocol.h \
	poll.c \
	rw.c \
	states.c \
	states.h \
	unlocked.h
libnbd_la_CPPFLAGS = \
	-I$(top_srcdir)/include
libnbd_la_CFLAGS = \
	$(WARNINGS_CFLAGS) \
	$(PTHREAD_CFLAGS)
libnbd_la_LDFLAGS = \
	$(PTHREAD_LIBS) \
	-version-info 0:0:0
