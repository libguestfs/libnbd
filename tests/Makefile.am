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

CLEANFILES += \
	connect-tcp.pid \
	connect-unix.pid \
	connect-unix.sock \
	connect-uri-tcp.pid \
	connect-uri-unix.pid \
	connect-uri-unix.sock

EXTRA_DIST = \
	aio-parallel.sh \
	aio-parallel-tls.sh \
	aio-parallel-load.sh \
	aio-parallel-load-tls.sh \
	eflags-plugin.sh \
	functions.sh.in \
	make-pki.sh \
	meta-base-allocation.sh \
	synch-parallel.sh \
	synch-parallel-tls.sh

if HAVE_NBDKIT

check_DATA =

check_PROGRAMS = \
	errors \
	server-death \
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
	can-not-multi-conn-flag \
	oldstyle \
	connect-unix \
	connect-tcp \
	connect-uri-unix \
	connect-uri-tcp \
	aio-parallel \
	aio-parallel-load \
	synch-parallel \
	meta-base-allocation
#	can-cache-flag
#	can-not-cache-flag

# Make sure that $srcdir is available to tests.
# Enable debug in all tests.
TESTS_ENVIRONMENT = srcdir=$(srcdir) LIBNBD_DEBUG=1
# Use the ./run script so we're always using the local library and tools.
LOG_COMPILER = $(top_builddir)/run

TESTS = \
	errors \
	server-death \
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
	can-not-multi-conn-flag \
	oldstyle \
	connect-unix \
	connect-tcp \
	connect-uri-unix \
	connect-uri-tcp \
	aio-parallel.sh \
	aio-parallel-load.sh \
	synch-parallel.sh \
	meta-base-allocation
#	can-cache-flag
#	can-not-cache-flag

errors_SOURCES = errors.c
errors_CPPFLAGS = -I$(top_srcdir)/include
errors_CFLAGS = $(WARNINGS_CFLAGS)
errors_LDADD = $(top_builddir)/lib/libnbd.la

server_death_SOURCES = server-death.c
server_death_CPPFLAGS = -I$(top_srcdir)/include
server_death_CFLAGS = $(WARNINGS_CFLAGS)
server_death_LDADD = $(top_builddir)/lib/libnbd.la

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

# Waiting for nbdkit to add support for can_cache.
#can_cache_flag_SOURCES = eflags.c
#can_cache_flag_CPPFLAGS = \
#	-I$(top_srcdir)/include -Dflag=can_cache -Dvalue=native
#can_cache_flag_CFLAGS = $(WARNINGS_CFLAGS)
#can_cache_flag_LDADD = $(top_builddir)/lib/libnbd.la
#
#can_not_cache_flag_SOURCES = eflags.c
#can_not_cache_flag_CPPFLAGS = \
#	-I$(top_srcdir)/include -Dflag=can_cache -Dvalue=none
#can_not_cache_flag_CFLAGS = $(WARNINGS_CFLAGS)
#can_not_cache_flag_LDADD = $(top_builddir)/lib/libnbd.la

oldstyle_SOURCES = oldstyle.c
oldstyle_CPPFLAGS = -I$(top_srcdir)/include
oldstyle_CFLAGS = $(WARNINGS_CFLAGS)
oldstyle_LDADD = $(top_builddir)/lib/libnbd.la

connect_unix_SOURCES = connect-unix.c
connect_unix_CPPFLAGS = -I$(top_srcdir)/include
connect_unix_CFLAGS = $(WARNINGS_CFLAGS)
connect_unix_LDADD = $(top_builddir)/lib/libnbd.la

connect_tcp_SOURCES = connect-tcp.c
connect_tcp_CPPFLAGS = -I$(top_srcdir)/include
connect_tcp_CFLAGS = $(WARNINGS_CFLAGS)
connect_tcp_LDADD = $(top_builddir)/lib/libnbd.la

connect_uri_unix_SOURCES = connect-uri-unix.c
connect_uri_unix_CPPFLAGS = -I$(top_srcdir)/include
connect_uri_unix_CFLAGS = $(WARNINGS_CFLAGS)
connect_uri_unix_LDADD = $(top_builddir)/lib/libnbd.la

connect_uri_tcp_SOURCES = connect-uri-tcp.c
connect_uri_tcp_CPPFLAGS = -I$(top_srcdir)/include
connect_uri_tcp_CFLAGS = $(WARNINGS_CFLAGS)
connect_uri_tcp_LDADD = $(top_builddir)/lib/libnbd.la

aio_parallel_SOURCES = aio-parallel.c
aio_parallel_CPPFLAGS = -I$(top_srcdir)/include
aio_parallel_CFLAGS = $(WARNINGS_CFLAGS) $(PTHREAD_CFLAGS)
aio_parallel_LDADD = $(top_builddir)/lib/libnbd.la $(PTHREAD_LIBS)

aio_parallel_load_SOURCES = aio-parallel-load.c
aio_parallel_load_CPPFLAGS = -I$(top_srcdir)/include
aio_parallel_load_CFLAGS = $(WARNINGS_CFLAGS) $(PTHREAD_CFLAGS)
aio_parallel_load_LDADD = $(top_builddir)/lib/libnbd.la $(PTHREAD_LIBS)

synch_parallel_SOURCES = synch-parallel.c
synch_parallel_CPPFLAGS = -I$(top_srcdir)/include
synch_parallel_CFLAGS = $(WARNINGS_CFLAGS) $(PTHREAD_CFLAGS)
synch_parallel_LDADD = $(top_builddir)/lib/libnbd.la $(PTHREAD_LIBS)

meta_base_allocation_SOURCES = meta-base-allocation.c
meta_base_allocation_CPPFLAGS = -I$(top_srcdir)/include
meta_base_allocation_CFLAGS = $(WARNINGS_CFLAGS)
meta_base_allocation_LDADD = $(top_builddir)/lib/libnbd.la

# Testing TLS support.
if HAVE_GNUTLS

if HAVE_CERTTOOL
check_PROGRAMS += connect-tls-certs
check_DATA += pki/stamp-pki
TESTS += connect-tls-certs

connect_tls_certs_SOURCES = connect-tls.c
connect_tls_certs_CPPFLAGS = -I$(top_srcdir)/include -DCERTS=1
connect_tls_certs_CFLAGS = $(WARNINGS_CFLAGS)
connect_tls_certs_LDADD = $(top_builddir)/lib/libnbd.la

pki/stamp-pki: $(srcdir)/make-pki.sh
	rm -rf pki pki-t
	SRCDIR=$(srcdir) CERTTOOL=$(CERTTOOL) $(srcdir)/make-pki.sh pki-t
	mv pki-t pki
	touch pki/stamp-pki

endif HAVE_CERTTOOL

if HAVE_PSKTOOL

check_PROGRAMS += \
	connect-tls-psk \
	aio-parallel-tls \
	aio-parallel-load-tls \
	synch-parallel-tls
TESTS += \
	connect-tls-psk \
	aio-parallel-tls.sh \
	aio-parallel-load-tls.sh \
	synch-parallel-tls.sh
check_DATA += keys.psk

connect_tls_psk_SOURCES = connect-tls.c
connect_tls_psk_CPPFLAGS = -I$(top_srcdir)/include -DPSK=1
connect_tls_psk_CFLAGS = $(WARNINGS_CFLAGS)
connect_tls_psk_LDADD = $(top_builddir)/lib/libnbd.la

aio_parallel_tls_SOURCES = aio-parallel.c
aio_parallel_tls_CPPFLAGS = -I$(top_srcdir)/include -DTLS=1
aio_parallel_tls_CFLAGS = $(WARNINGS_CFLAGS) $(PTHREAD_CFLAGS)
aio_parallel_tls_LDADD = $(top_builddir)/lib/libnbd.la $(PTHREAD_LIBS)

aio_parallel_load_tls_SOURCES = aio-parallel-load.c
aio_parallel_load_tls_CPPFLAGS = -I$(top_srcdir)/include -DTLS=1
aio_parallel_load_tls_CFLAGS = $(WARNINGS_CFLAGS) $(PTHREAD_CFLAGS)
aio_parallel_load_tls_LDADD = $(top_builddir)/lib/libnbd.la $(PTHREAD_LIBS)

synch_parallel_tls_SOURCES = synch-parallel.c
synch_parallel_tls_CPPFLAGS = -I$(top_srcdir)/include -DTLS=1
synch_parallel_tls_CFLAGS = $(WARNINGS_CFLAGS) $(PTHREAD_CFLAGS)
synch_parallel_tls_LDADD = $(top_builddir)/lib/libnbd.la $(PTHREAD_LIBS)

keys.psk:
	rm -f $@ $@-t
	$(PSKTOOL) -u $(LOGNAME) -p $@-t
	mv $@-t $@

endif HAVE_PSKTOOL

endif HAVE_GNUTLS

# Keys are expensive to recreate, so only delete them when we do
# ‘make distclean’.
DISTCLEANFILES = keys.psk
distclean-local: distclean-local-tls
distclean-local-tls:
	rm -rf pki

endif HAVE_NBDKIT

check-valgrind:
	LIBNBD_VALGRIND=1 $(MAKE) check
