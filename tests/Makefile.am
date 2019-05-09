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

EXTRA_DIST = \
	eflags-plugin.sh

check_PROGRAMS = \
	get-size \
	read-only-flag \
	read-write-flag \
	can-flush-flag \
	can-not-flush-flag \
	can-fua-flag \
	can-not-fua-flag \
	is-rotational-flag \
	is-not-rotational-flag \
	can-trim-flag \
	can-not-trim-flag \
	can-zero-flag \
	can-multi-conn-flag \
	can-not-multi-conn-flag

# Make sure that $srcdir is available to tests.
TESTS_ENVIRONMENT = srcdir=$(srcdir)

TESTS = \
	get-size \
	read-only-flag \
	read-write-flag \
	can-flush-flag \
	can-not-flush-flag \
	can-fua-flag \
	can-not-fua-flag \
	is-rotational-flag \
	is-not-rotational-flag \
	can-trim-flag \
	can-not-trim-flag \
	can-zero-flag \
	can-multi-conn-flag \
	can-not-multi-conn-flag

get_size_SOURCES = get-size.c
get_size_CPPFLAGS = -I$(top_srcdir)/include
get_size_CFLAGS = $(WARNINGS_CFLAGS)
get_size_LDADD = $(top_builddir)/lib/libnbd.la

read_only_flag_SOURCES = read-only-flag.c
read_only_flag_CPPFLAGS = -I$(top_srcdir)/include
read_only_flag_CFLAGS = $(WARNINGS_CFLAGS)
read_only_flag_LDADD = $(top_builddir)/lib/libnbd.la

read_write_flag_SOURCES = read-write-flag.c
read_write_flag_CPPFLAGS = -I$(top_srcdir)/include
read_write_flag_CFLAGS = $(WARNINGS_CFLAGS)
read_write_flag_LDADD = $(top_builddir)/lib/libnbd.la

can_flush_flag_SOURCES = eflags.c
can_flush_flag_CPPFLAGS = \
	-I$(top_srcdir)/include -Dflag=can_flush
can_flush_flag_CFLAGS = $(WARNINGS_CFLAGS)
can_flush_flag_LDADD = $(top_builddir)/lib/libnbd.la

can_not_flush_flag_SOURCES = eflags.c
can_not_flush_flag_CPPFLAGS = \
	-I$(top_srcdir)/include -Dflag=can_flush -Dvalue=false
can_not_flush_flag_CFLAGS = $(WARNINGS_CFLAGS)
can_not_flush_flag_LDADD = $(top_builddir)/lib/libnbd.la

can_fua_flag_SOURCES = eflags.c
can_fua_flag_CPPFLAGS = \
	-I$(top_srcdir)/include -Dflag=can_fua -Dvalue=native
can_fua_flag_CFLAGS = $(WARNINGS_CFLAGS)
can_fua_flag_LDADD = $(top_builddir)/lib/libnbd.la

can_not_fua_flag_SOURCES = eflags.c
can_not_fua_flag_CPPFLAGS = \
	-I$(top_srcdir)/include -Dflag=can_fua -Dvalue=none
can_not_fua_flag_CFLAGS = $(WARNINGS_CFLAGS)
can_not_fua_flag_LDADD = $(top_builddir)/lib/libnbd.la

is_rotational_flag_SOURCES = eflags.c
is_rotational_flag_CPPFLAGS = \
	-I$(top_srcdir)/include -Dflag=is_rotational
is_rotational_flag_CFLAGS = $(WARNINGS_CFLAGS)
is_rotational_flag_LDADD = $(top_builddir)/lib/libnbd.la

is_not_rotational_flag_SOURCES = eflags.c
is_not_rotational_flag_CPPFLAGS = \
	-I$(top_srcdir)/include -Dflag=is_rotational -Dvalue=false
is_not_rotational_flag_CFLAGS = $(WARNINGS_CFLAGS)
is_not_rotational_flag_LDADD = $(top_builddir)/lib/libnbd.la

can_trim_flag_SOURCES = eflags.c
can_trim_flag_CPPFLAGS = \
	-I$(top_srcdir)/include -Dflag=can_trim
can_trim_flag_CFLAGS = $(WARNINGS_CFLAGS)
can_trim_flag_LDADD = $(top_builddir)/lib/libnbd.la

can_not_trim_flag_SOURCES = eflags.c
can_not_trim_flag_CPPFLAGS = \
	-I$(top_srcdir)/include -Dflag=can_trim -Dvalue=false
can_not_trim_flag_CFLAGS = $(WARNINGS_CFLAGS)
can_not_trim_flag_LDADD = $(top_builddir)/lib/libnbd.la

can_zero_flag_SOURCES = eflags.c
can_zero_flag_CPPFLAGS = \
	-I$(top_srcdir)/include -Dflag=can_zero
can_zero_flag_CFLAGS = $(WARNINGS_CFLAGS)
can_zero_flag_LDADD = $(top_builddir)/lib/libnbd.la
# Note: can't test not-zero case because nbdkit emulates zero if the
# plugin can't do it.

can_multi_conn_flag_SOURCES = eflags.c
can_multi_conn_flag_CPPFLAGS = \
	-I$(top_srcdir)/include -Dflag=can_multi_conn
can_multi_conn_flag_CFLAGS = $(WARNINGS_CFLAGS)
can_multi_conn_flag_LDADD = $(top_builddir)/lib/libnbd.la

can_not_multi_conn_flag_SOURCES = eflags.c
can_not_multi_conn_flag_CPPFLAGS = \
	-I$(top_srcdir)/include -Dflag=can_multi_conn -Dvalue=false
can_not_multi_conn_flag_CFLAGS = $(WARNINGS_CFLAGS)
can_not_multi_conn_flag_LDADD = $(top_builddir)/lib/libnbd.la

endif HAVE_NBDKIT
