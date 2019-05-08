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

if HAVE_NBDKIT

check_PROGRAMS = \
	get-size

TESTS = \
	get-size

get_size_SOURCES = \
	get-size.c
get_size_CPPFLAGS = \
	-I$(top_srcdir)/include
get_size_CFLAGS = \
	$(WARNINGS_CFLAGS)
get_size_LDADD = \
	$(top_builddir)/lib/libnbd.la

endif HAVE_NBDKIT
