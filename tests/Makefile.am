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
	connect-uri-nbd.pid \
	connect-uri-nbd-unix.pid \
	connect-uri-nbds.pid \
	connect-uri-nbds-unix.pid \
	connect-uri-nbds-psk.pid \
	$(NULL)

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
	synch-parallel-tls.sh \
	$(NULL)

check_DATA =
check_PROGRAMS =
TESTS =

# Make sure that $srcdir is available to tests.
# Enable debug in all tests.
TESTS_ENVIRONMENT = srcdir=$(srcdir) LIBNBD_DEBUG=1
# Use the ./run script so we're always using the local library and tools.
LOG_COMPILER = $(top_builddir)/run

#----------------------------------------------------------------------
# The following tests do not need an NBD server.

check_PROGRAMS += \
	compile-header-only \
	compile-c \
	compile-ansi-c \
	close-null \
	debug \
	debug-environment \
	version \
	export-name \
	$(NULL)

TESTS += \
	compile-header-only \
	compile-c \
	compile-ansi-c \
	close-null \
	debug \
	debug-environment \
	version \
	export-name \
	$(NULL)

# Even though we have a compile.c, we do not want make to create a 'compile'
.PHONY: compile

compile_header_only_SOURCES = compile-header-only.c
compile_header_only_CPPFLAGS = -I$(top_srcdir)/include
compile_header_only_CFLAGS = $(WARNINGS_CFLAGS)
compile_header_only_LDADD = $(top_builddir)/lib/libnbd.la

compile_c_SOURCES = compile.c
compile_c_CPPFLAGS = -I$(top_srcdir)/include
compile_c_CFLAGS = $(WARNINGS_CFLAGS)
compile_c_LDADD = $(top_builddir)/lib/libnbd.la

compile_ansi_c_SOURCES = compile-ansi-c.c
compile_ansi_c_CPPFLAGS = \
	-I$(top_srcdir)/include \
	-std=c90 -pedantic
compile_ansi_c_CFLAGS = $(WARNINGS_CFLAGS)
compile_ansi_c_LDADD = $(top_builddir)/lib/libnbd.la

close_null_SOURCES = close-null.c
close_null_CPPFLAGS = -I$(top_srcdir)/include
close_null_CFLAGS = $(WARNINGS_CFLAGS)
close_null_LDADD = $(top_builddir)/lib/libnbd.la

debug_SOURCES = debug.c
debug_CPPFLAGS = -I$(top_srcdir)/include
debug_CFLAGS = $(WARNINGS_CFLAGS)
debug_LDADD = $(top_builddir)/lib/libnbd.la

debug_environment_SOURCES = debug-environment.c
debug_environment_CPPFLAGS = -I$(top_srcdir)/include
debug_environment_CFLAGS = $(WARNINGS_CFLAGS)
debug_environment_LDADD = $(top_builddir)/lib/libnbd.la

version_SOURCES = version.c
version_CPPFLAGS = -I$(top_srcdir)/include
version_CFLAGS = $(WARNINGS_CFLAGS)
version_LDADD = $(top_builddir)/lib/libnbd.la

export_name_SOURCES = export-name.c
export_name_CPPFLAGS = -I$(top_srcdir)/include
export_name_CFLAGS = $(WARNINGS_CFLAGS)
export_name_LDADD = $(top_builddir)/lib/libnbd.la

if HAVE_CXX

check_PROGRAMS += compile-cxx
TESTS += compile-cxx

compile_cxx_SOURCES = compile-cxx.cpp
compile_cxx_CPPFLAGS = -I$(top_srcdir)/include
compile_cxx_CXXFLAGS = $(WARNINGS_CFLAGS)
compile_cxx_LDADD = $(top_builddir)/lib/libnbd.la

endif HAVE_CXX

#----------------------------------------------------------------------
# The following tests require nbdkit as an NBD server to test against.

if HAVE_NBDKIT

check_PROGRAMS += \
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
	can-not-zero-flag \
	can-fast-zero-flag \
	can-not-fast-zero-flag \
	can-df-flag \
	can-not-df-flag \
	can-multi-conn-flag \
	can-not-multi-conn-flag \
	can-cache-flag \
	can-not-cache-flag \
	oldstyle \
	connect-unix \
	connect-tcp \
	aio-parallel \
	aio-parallel-load \
	synch-parallel \
	meta-base-allocation \
	closure-lifetimes \
	$(NULL)

TESTS += \
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
	can-not-zero-flag \
	can-fast-zero-flag \
	can-not-fast-zero-flag \
	can-df-flag \
	can-not-df-flag \
	can-multi-conn-flag \
	can-not-multi-conn-flag \
	can-cache-flag \
	can-not-cache-flag \
	oldstyle \
	connect-unix \
	connect-tcp \
	aio-parallel.sh \
	aio-parallel-load.sh \
	synch-parallel.sh \
	meta-base-allocation \
	closure-lifetimes \
	$(NULL)

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
	-I$(top_srcdir)/include -Dflag=can_flush \
	$(NULL)
can_flush_flag_CFLAGS = $(WARNINGS_CFLAGS)
can_flush_flag_LDADD = $(top_builddir)/lib/libnbd.la

can_not_flush_flag_SOURCES = eflags.c
can_not_flush_flag_CPPFLAGS = \
	-I$(top_srcdir)/include -Dflag=can_flush -Dvalue=false \
	$(NULL)
can_not_flush_flag_CFLAGS = $(WARNINGS_CFLAGS)
can_not_flush_flag_LDADD = $(top_builddir)/lib/libnbd.la

can_fua_flag_SOURCES = eflags.c
can_fua_flag_CPPFLAGS = \
	-I$(top_srcdir)/include -Dflag=can_fua -Dvalue=native \
	$(NULL)
can_fua_flag_CFLAGS = $(WARNINGS_CFLAGS)
can_fua_flag_LDADD = $(top_builddir)/lib/libnbd.la

can_not_fua_flag_SOURCES = eflags.c
can_not_fua_flag_CPPFLAGS = \
	-I$(top_srcdir)/include -Dflag=can_fua -Dvalue=none \
	$(NULL)
can_not_fua_flag_CFLAGS = $(WARNINGS_CFLAGS)
can_not_fua_flag_LDADD = $(top_builddir)/lib/libnbd.la

is_rotational_flag_SOURCES = eflags.c
is_rotational_flag_CPPFLAGS = \
	-I$(top_srcdir)/include -Dflag=is_rotational \
	$(NULL)
is_rotational_flag_CFLAGS = $(WARNINGS_CFLAGS)
is_rotational_flag_LDADD = $(top_builddir)/lib/libnbd.la

is_not_rotational_flag_SOURCES = eflags.c
is_not_rotational_flag_CPPFLAGS = \
	-I$(top_srcdir)/include -Dflag=is_rotational -Dvalue=false \
	$(NULL)
is_not_rotational_flag_CFLAGS = $(WARNINGS_CFLAGS)
is_not_rotational_flag_LDADD = $(top_builddir)/lib/libnbd.la

can_trim_flag_SOURCES = eflags.c
can_trim_flag_CPPFLAGS = \
	-I$(top_srcdir)/include -Dflag=can_trim \
	$(NULL)
can_trim_flag_CFLAGS = $(WARNINGS_CFLAGS)
can_trim_flag_LDADD = $(top_builddir)/lib/libnbd.la

can_not_trim_flag_SOURCES = eflags.c
can_not_trim_flag_CPPFLAGS = \
	-I$(top_srcdir)/include -Dflag=can_trim -Dvalue=false \
	$(NULL)
can_not_trim_flag_CFLAGS = $(WARNINGS_CFLAGS)
can_not_trim_flag_LDADD = $(top_builddir)/lib/libnbd.la

can_zero_flag_SOURCES = eflags.c
can_zero_flag_CPPFLAGS = \
	-I$(top_srcdir)/include -Dflag=can_zero \
	$(NULL)
can_zero_flag_CFLAGS = $(WARNINGS_CFLAGS)
can_zero_flag_LDADD = $(top_builddir)/lib/libnbd.la

can_not_zero_flag_SOURCES = eflags.c
can_not_zero_flag_CPPFLAGS = \
	-I$(top_srcdir)/include -Dflag=can_zero -Dvalue=false \
	-Dfilter='"--filter=nozero"' \
	$(NULL)
can_not_zero_flag_CFLAGS = $(WARNINGS_CFLAGS)
can_not_zero_flag_LDADD = $(top_builddir)/lib/libnbd.la

can_fast_zero_flag_SOURCES = eflags.c
can_fast_zero_flag_CPPFLAGS = \
	-I$(top_srcdir)/include -Dflag=can_fast_zero \
	-Drequire='"has_can_fast_zero=1"' \
	$(NULL)
can_fast_zero_flag_CFLAGS = $(WARNINGS_CFLAGS)
can_fast_zero_flag_LDADD = $(top_builddir)/lib/libnbd.la

can_not_fast_zero_flag_SOURCES = eflags.c
can_not_fast_zero_flag_CPPFLAGS = \
	-I$(top_srcdir)/include -Dflag=can_fast_zero -Dvalue=false \
	-Drequire='"has_can_fast_zero=1"' \
	$(NULL)
can_not_fast_zero_flag_CFLAGS = $(WARNINGS_CFLAGS)
can_not_fast_zero_flag_LDADD = $(top_builddir)/lib/libnbd.la

can_df_flag_SOURCES = eflags.c
can_df_flag_CPPFLAGS = \
	-I$(top_srcdir)/include -Dflag=can_df \
	$(NULL)
can_df_flag_CFLAGS = $(WARNINGS_CFLAGS)
can_df_flag_LDADD = $(top_builddir)/lib/libnbd.la

can_not_df_flag_SOURCES = eflags.c
can_not_df_flag_CPPFLAGS = \
	-I$(top_srcdir)/include -Dflag=can_df -Dvalue=false -Dno_sr \
	$(NULL)
can_not_df_flag_CFLAGS = $(WARNINGS_CFLAGS)
can_not_df_flag_LDADD = $(top_builddir)/lib/libnbd.la

can_multi_conn_flag_SOURCES = eflags.c
can_multi_conn_flag_CPPFLAGS = \
	-I$(top_srcdir)/include -Dflag=can_multi_conn \
	$(NULL)
can_multi_conn_flag_CFLAGS = $(WARNINGS_CFLAGS)
can_multi_conn_flag_LDADD = $(top_builddir)/lib/libnbd.la

can_not_multi_conn_flag_SOURCES = eflags.c
can_not_multi_conn_flag_CPPFLAGS = \
	-I$(top_srcdir)/include -Dflag=can_multi_conn -Dvalue=false \
	$(NULL)
can_not_multi_conn_flag_CFLAGS = $(WARNINGS_CFLAGS)
can_not_multi_conn_flag_LDADD = $(top_builddir)/lib/libnbd.la

can_cache_flag_SOURCES = eflags.c
can_cache_flag_CPPFLAGS = \
	-I$(top_srcdir)/include -Dflag=can_cache -Dvalue=native \
	-Drequire='"has_can_cache=1"' \
	$(NULL)
can_cache_flag_CFLAGS = $(WARNINGS_CFLAGS)
can_cache_flag_LDADD = $(top_builddir)/lib/libnbd.la

can_not_cache_flag_SOURCES = eflags.c
can_not_cache_flag_CPPFLAGS = \
	-I$(top_srcdir)/include -Dflag=can_cache -Dvalue=none \
	-Drequire='"has_can_cache=1"' \
	$(NULL)
can_not_cache_flag_CFLAGS = $(WARNINGS_CFLAGS)
can_not_cache_flag_LDADD = $(top_builddir)/lib/libnbd.la

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

aio_parallel_SOURCES = aio-parallel.c
aio_parallel_CPPFLAGS = \
	-I$(top_srcdir)/include \
	-I$(top_srcdir)/common/include \
	$(NULL)
aio_parallel_CFLAGS = $(WARNINGS_CFLAGS) $(PTHREAD_CFLAGS)
aio_parallel_LDADD = $(top_builddir)/lib/libnbd.la $(PTHREAD_LIBS)

aio_parallel_load_SOURCES = aio-parallel-load.c
aio_parallel_load_CPPFLAGS = -I$(top_srcdir)/include
aio_parallel_load_CFLAGS = $(WARNINGS_CFLAGS) $(PTHREAD_CFLAGS)
aio_parallel_load_LDADD = $(top_builddir)/lib/libnbd.la $(PTHREAD_LIBS)

synch_parallel_SOURCES = synch-parallel.c
synch_parallel_CPPFLAGS = \
	-I$(top_srcdir)/include \
	-I$(top_srcdir)/common/include \
	$(NULL)
synch_parallel_CFLAGS = $(WARNINGS_CFLAGS) $(PTHREAD_CFLAGS)
synch_parallel_LDADD = $(top_builddir)/lib/libnbd.la $(PTHREAD_LIBS)

meta_base_allocation_SOURCES = meta-base-allocation.c
meta_base_allocation_CPPFLAGS = -I$(top_srcdir)/include
meta_base_allocation_CFLAGS = $(WARNINGS_CFLAGS)
meta_base_allocation_LDADD = $(top_builddir)/lib/libnbd.la

closure_lifetimes_SOURCES = closure-lifetimes.c
closure_lifetimes_CPPFLAGS = -I$(top_srcdir)/include
closure_lifetimes_CFLAGS = $(WARNINGS_CFLAGS)
closure_lifetimes_LDADD = $(top_builddir)/lib/libnbd.la

#----------------------------------------------------------------------
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
	synch-parallel-tls \
	$(NULL)
TESTS += \
	connect-tls-psk \
	aio-parallel-tls.sh \
	aio-parallel-load-tls.sh \
	synch-parallel-tls.sh \
	$(NULL)
check_DATA += keys.psk

connect_tls_psk_SOURCES = connect-tls.c
connect_tls_psk_CPPFLAGS = -I$(top_srcdir)/include -DPSK=1
connect_tls_psk_CFLAGS = $(WARNINGS_CFLAGS)
connect_tls_psk_LDADD = $(top_builddir)/lib/libnbd.la

aio_parallel_tls_SOURCES = aio-parallel.c
aio_parallel_tls_CPPFLAGS = \
	-I$(top_srcdir)/include \
	-I$(top_srcdir)/common/include \
	-DTLS=1 \
	$(NULL)
aio_parallel_tls_CFLAGS = $(WARNINGS_CFLAGS) $(PTHREAD_CFLAGS)
aio_parallel_tls_LDADD = $(top_builddir)/lib/libnbd.la $(PTHREAD_LIBS)

aio_parallel_load_tls_SOURCES = aio-parallel-load.c
aio_parallel_load_tls_CPPFLAGS = -I$(top_srcdir)/include -DTLS=1
aio_parallel_load_tls_CFLAGS = $(WARNINGS_CFLAGS) $(PTHREAD_CFLAGS)
aio_parallel_load_tls_LDADD = $(top_builddir)/lib/libnbd.la $(PTHREAD_LIBS)

synch_parallel_tls_SOURCES = synch-parallel.c
synch_parallel_tls_CPPFLAGS = \
	-I$(top_srcdir)/include \
	-I$(top_srcdir)/common/include \
	-DTLS=1 \
	$(NULL)
synch_parallel_tls_CFLAGS = $(WARNINGS_CFLAGS) $(PTHREAD_CFLAGS)
synch_parallel_tls_LDADD = $(top_builddir)/lib/libnbd.la $(PTHREAD_LIBS)

keys.psk:
	rm -f $@ $@-t
	$(PSKTOOL) -u alice -p $@-t
	mv $@-t $@

endif HAVE_PSKTOOL

endif HAVE_GNUTLS

#----------------------------------------------------------------------
# Testing URIs.

check_PROGRAMS += \
	connect-uri-nbd \
	connect-uri-nbd-unix \
	$(NULL)
TESTS += \
	connect-uri-nbd \
	connect-uri-nbd-unix \
	$(NULL)

RANDOM1 := $(shell bash -c "echo $$(( 32768 + (RANDOM & 16383) ))")
connect_uri_nbd_SOURCES = connect-uri.c
connect_uri_nbd_CPPFLAGS = \
	-I$(top_srcdir)/include \
	-DSERVER_PARAMS='"-p", "$(RANDOM1)"' \
	-DPIDFILE='"connect-uri-nbd.pid"' \
	-DURI='"nbd://localhost:$(RANDOM1)/"'
connect_uri_nbd_CFLAGS = $(WARNINGS_CFLAGS)
connect_uri_nbd_LDADD = $(top_builddir)/lib/libnbd.la

CONNECT_URI_NBD_UNIX_SOCKET := \
	$(shell mktemp /tmp/connect-uri-nbd-unix-socket-XXXXXX)
connect_uri_nbd_unix_SOURCES = connect-uri.c
connect_uri_nbd_unix_CPPFLAGS = \
	-I$(top_srcdir)/include \
	-DSERVER_PARAMS='"-U", SOCKET' \
	-DSOCKET='"$(CONNECT_URI_NBD_UNIX_SOCKET)"' \
	-DPIDFILE='"connect-uri-nbd-unix.pid"' \
	-DURI='"nbd+unix:///?socket=" SOCKET'
connect_uri_nbd_unix_CFLAGS = $(WARNINGS_CFLAGS)
connect_uri_nbd_unix_LDADD = $(top_builddir)/lib/libnbd.la

if HAVE_GNUTLS
if HAVE_CERTTOOL

check_PROGRAMS += \
	connect-uri-nbds \
	connect-uri-nbds-unix \
	$(NULL)
TESTS += \
	connect-uri-nbds \
	connect-uri-nbds-unix \
	$(NULL)

RANDOM2 := $(shell bash -c "echo $$(( 32768 + (RANDOM & 16383) ))")
connect_uri_nbds_SOURCES = connect-uri.c
connect_uri_nbds_CPPFLAGS = \
	-I$(top_srcdir)/include \
	-DSERVER_PARAMS='"-p", "$(RANDOM2)", "--tls=require", "--tls-certificates=pki"' \
	-DPIDFILE='"connect-uri-nbds.pid"' \
	-DURI='"nbds://localhost:$(RANDOM2)/"'
connect_uri_nbds_CFLAGS = $(WARNINGS_CFLAGS)
connect_uri_nbds_LDADD = $(top_builddir)/lib/libnbd.la

CONNECT_URI_NBDS_UNIX_SOCKET := \
	$(shell mktemp /tmp/connect-uri-nbds-unix-socket-XXXXXX)
connect_uri_nbds_unix_SOURCES = connect-uri.c
connect_uri_nbds_unix_CPPFLAGS = \
	-I$(top_srcdir)/include \
	-DSERVER_PARAMS='"-U", SOCKET, "--tls=require", "--tls-certificates=pki"' \
	-DSOCKET='"$(CONNECT_URI_NBDS_UNIX_SOCKET)"' \
	-DPIDFILE='"connect-uri-nbds-unix.pid"' \
	-DURI='"nbds+unix:///?socket=" SOCKET'
connect_uri_nbds_unix_CFLAGS = $(WARNINGS_CFLAGS)
connect_uri_nbds_unix_LDADD = $(top_builddir)/lib/libnbd.la

endif HAVE_CERTTOOL

if HAVE_PSKTOOL

check_PROGRAMS += \
	connect-uri-nbds-psk \
	$(NULL)
TESTS += \
	connect-uri-nbds-psk \
	$(NULL)

RANDOM3 := $(shell bash -c "echo $$(( 32768 + (RANDOM & 16383) ))")
connect_uri_nbds_psk_SOURCES = connect-uri.c
connect_uri_nbds_psk_CPPFLAGS = \
	-I$(top_srcdir)/include \
	-DSERVER_PARAMS='"-p", "$(RANDOM3)", "--tls=require", "--tls-psk=keys.psk"' \
	-DPIDFILE='"connect-uri-nbds-psk.pid"' \
	-DURI='"nbds://alice@localhost:$(RANDOM3)/?tls-psk-file=keys.psk"'
connect_uri_nbds_psk_CFLAGS = $(WARNINGS_CFLAGS)
connect_uri_nbds_psk_LDADD = $(top_builddir)/lib/libnbd.la

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
