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

noinst_PROGRAMS = \
	simple-fetch-first-sector \
	simple-reads-and-writes \
	threaded-reads-and-writes

simple_fetch_first_sector_SOURCES = \
	simple-fetch-first-sector.c
simple_fetch_first_sector_CPPFLAGS = \
	-I$(top_srcdir)/include
simple_fetch_first_sector_CFLAGS = \
	$(WARNINGS_CFLAGS)
simple_fetch_first_sector_LDADD = \
	$(top_builddir)/lib/libnbd.la

simple_reads_and_writes_SOURCES = \
	simple-reads-and-writes.c
simple_reads_and_writes_CPPFLAGS = \
	-I$(top_srcdir)/include
simple_reads_and_writes_CFLAGS = \
	$(WARNINGS_CFLAGS)
simple_reads_and_writes_LDADD = \
	$(top_builddir)/lib/libnbd.la

threaded_reads_and_writes_SOURCES = \
	threaded-reads-and-writes.c
threaded_reads_and_writes_CPPFLAGS = \
	-I$(top_srcdir)/include
threaded_reads_and_writes_CFLAGS = \
	$(WARNINGS_CFLAGS) \
	$(PTHREAD_CFLAGS)
threaded_reads_and_writes_LDADD = \
	$(top_builddir)/lib/libnbd.la \
	$(PTHREAD_LIBS)
