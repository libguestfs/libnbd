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

EXTRA_DIST = LICENSE-FOR-EXAMPLES

noinst_PROGRAMS = \
	batched-read-write \
	simple-fetch-first-sector \
	simple-reads-and-writes \
	threaded-reads-and-writes \
	$(NULL)

simple_fetch_first_sector_SOURCES = \
	simple-fetch-first-sector.c \
	$(NULL)
simple_fetch_first_sector_CPPFLAGS = \
	-I$(top_srcdir)/include \
	$(NULL)
simple_fetch_first_sector_CFLAGS = \
	$(WARNINGS_CFLAGS) \
	$(NULL)
simple_fetch_first_sector_LDADD = \
	$(top_builddir)/lib/libnbd.la \
	$(NULL)

simple_reads_and_writes_SOURCES = \
	simple-reads-and-writes.c \
	$(NULL)
simple_reads_and_writes_CPPFLAGS = \
	-I$(top_srcdir)/include \
	$(NULL)
simple_reads_and_writes_CFLAGS = \
	$(WARNINGS_CFLAGS) \
	$(NULL)
simple_reads_and_writes_LDADD = \
	$(top_builddir)/lib/libnbd.la \
	$(NULL)

threaded_reads_and_writes_SOURCES = \
	threaded-reads-and-writes.c \
	$(NULL)
threaded_reads_and_writes_CPPFLAGS = \
	-I$(top_srcdir)/include \
	$(NULL)
threaded_reads_and_writes_CFLAGS = \
	$(WARNINGS_CFLAGS) \
	$(PTHREAD_CFLAGS) \
	$(NULL)
threaded_reads_and_writes_LDADD = \
	$(top_builddir)/lib/libnbd.la \
	$(PTHREAD_LIBS) \
	$(NULL)

batched_read_write_SOURCES = \
	batched-read-write.c \
	$(NULL)
batched_read_write_CPPFLAGS = \
	-I$(top_srcdir)/include \
	$(NULL)
batched_read_write_CFLAGS = \
	$(WARNINGS_CFLAGS) \
	$(NULL)
batched_read_write_LDADD = \
	$(top_builddir)/lib/libnbd.la \
	$(NULL)
