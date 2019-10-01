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

EXTRA_DIST = \
	dirty-bitmap.sh \
	structured-read.sh \
	$(NULL)

TESTS_ENVIRONMENT = LIBNBD_DEBUG=1
LOG_COMPILER = $(top_builddir)/run
check_PROGRAMS =
TESTS =

if HAVE_NBD_SERVER

check_PROGRAMS += interop-nbd-server
TESTS += interop-nbd-server

interop_nbd_server_SOURCES = interop.c
interop_nbd_server_CPPFLAGS = \
	-I$(top_srcdir)/include \
	-DSERVER=\"$(NBD_SERVER)\" \
	-DSERVER_PARAMS='"-d", "-C", "/dev/null", "0", tmpfile' \
	-DEXPORT_NAME='""'
interop_nbd_server_CFLAGS = $(WARNINGS_CFLAGS)
interop_nbd_server_LDADD = $(top_builddir)/lib/libnbd.la

endif HAVE_NBD_SERVER

if HAVE_QEMU_NBD

check_PROGRAMS += \
	interop-qemu-nbd \
	dirty-bitmap \
	socket-activation-qemu-nbd \
	structured-read \
	$(NULL)
TESTS += \
	interop-qemu-nbd \
	dirty-bitmap.sh \
	socket-activation-qemu-nbd \
	structured-read.sh \
	$(NULL)

# tls tests assume the pre-existence of files created in ../tests/Makefile.am,
# so we can only run them under the same conditions used there
# XXX Make the tls file creation independent of HAVE_NBDKIT
if HAVE_NBDKIT
if HAVE_GNUTLS
if HAVE_CERTTOOL
check_PROGRAMS += \
	interop-qemu-nbd-tls-certs \
	$(NULL)
TESTS += \
	interop-qemu-nbd-tls-certs \
	$(NULL)
endif
if HAVE_PSKTOOL
check_PROGRAMS += \
	interop-qemu-nbd-tls-psk \
	$(NULL)
TESTS += \
	interop-qemu-nbd-tls-psk \
	$(NULL)
endif
endif
endif

interop_qemu_nbd_SOURCES = interop.c
interop_qemu_nbd_CPPFLAGS = \
	-I$(top_srcdir)/include \
	-DSOCKET_ACTIVATION=1 \
	-DSERVER=\"$(QEMU_NBD)\" \
	-DSERVER_PARAMS='"-f", "raw", "-x", "/", tmpfile' \
	-DEXPORT_NAME='"/"' \
	$(NULL)
interop_qemu_nbd_CFLAGS = $(WARNINGS_CFLAGS)
interop_qemu_nbd_LDADD = $(top_builddir)/lib/libnbd.la

# qemu-nbd requires absolute path to dir
interop_qemu_nbd_tls_certs_SOURCES = interop.c
interop_qemu_nbd_tls_certs_CPPFLAGS = \
	-I$(top_srcdir)/include \
	-DSOCKET_ACTIVATION=1 \
	-DSERVER=\"$(QEMU_NBD)\" \
	-DSERVER_PARAMS='"--object", "tls-creds-x509,id=tls0,endpoint=server,dir=$(abs_top_builddir)/tests/pki", "--tls-creds", "tls0", "-f", "raw", "-x", "/", tmpfile' \
	-DEXPORT_NAME='"/"' \
	-DCERTS=1 \
	-DTLS_MODE=LIBNBD_TLS_REQUIRE \
	$(NULL)
interop_qemu_nbd_tls_certs_CFLAGS = $(WARNINGS_CFLAGS)
interop_qemu_nbd_tls_certs_LDADD = $(top_builddir)/lib/libnbd.la

# qemu-nbd requires absolute path to dir
interop_qemu_nbd_tls_psk_SOURCES = interop.c
interop_qemu_nbd_tls_psk_CPPFLAGS = \
	-I$(top_srcdir)/include \
	-DSOCKET_ACTIVATION=1 \
	-DSERVER=\"$(QEMU_NBD)\" \
	-DSERVER_PARAMS='"--object", "tls-creds-psk,id=tls0,endpoint=server,dir=$(abs_top_builddir)/tests", "--tls-creds", "tls0", "-f", "raw", "-x", "/", tmpfile' \
	-DEXPORT_NAME='"/"' \
	-DPSK=1 \
	-DTLS_MODE=LIBNBD_TLS_REQUIRE \
	$(NULL)
interop_qemu_nbd_tls_psk_CFLAGS = $(WARNINGS_CFLAGS)
interop_qemu_nbd_tls_psk_LDADD = $(top_builddir)/lib/libnbd.la

dirty_bitmap_SOURCES = dirty-bitmap.c
dirty_bitmap_CPPFLAGS = -I$(top_srcdir)/include
dirty_bitmap_CFLAGS = $(WARNINGS_CFLAGS)
dirty_bitmap_LDADD = $(top_builddir)/lib/libnbd.la

socket_activation_qemu_nbd_SOURCES = socket-activation.c
socket_activation_qemu_nbd_CPPFLAGS = \
	-I$(top_srcdir)/include \
	-DSERVER=\"$(QEMU_NBD)\" \
	-DSERVER_PARAMS='"-f", "raw", "-x", "", tmpfile' \
	$(NULL)
socket_activation_qemu_nbd_CFLAGS = $(WARNINGS_CFLAGS)
socket_activation_qemu_nbd_LDADD = $(top_builddir)/lib/libnbd.la

structured_read_SOURCES = structured-read.c
structured_read_CPPFLAGS = -I$(top_srcdir)/include
structured_read_CFLAGS = $(WARNINGS_CFLAGS)
structured_read_LDADD = $(top_builddir)/lib/libnbd.la

endif HAVE_QEMU_NBD

if HAVE_NBDKIT

check_PROGRAMS += \
	interop-nbdkit \
	socket-activation-nbdkit \
	$(NULL)
TESTS += \
	interop-nbdkit \
	socket-activation-nbdkit \
	$(NULL)

# See above comment about files in ../tests/Makefile.am
if HAVE_GNUTLS
if HAVE_CERTTOOL
check_PROGRAMS += \
	interop-nbdkit-tls-certs \
	interop-nbdkit-tls-certs-allow-enabled \
	interop-nbdkit-tls-certs-allow-fallback \
	$(NULL)
TESTS += \
	interop-nbdkit-tls-certs \
	interop-nbdkit-tls-certs-allow-enabled \
	interop-nbdkit-tls-certs-allow-fallback \
	$(NULL)
endif
if HAVE_PSKTOOL
check_PROGRAMS += \
	interop-nbdkit-tls-psk \
	interop-nbdkit-tls-psk-allow-enabled \
	interop-nbdkit-tls-psk-allow-fallback \
	$(NULL)
TESTS += \
	interop-nbdkit-tls-psk \
	interop-nbdkit-tls-psk-allow-enabled \
	interop-nbdkit-tls-psk-allow-fallback \
	$(NULL)
endif
endif

interop_nbdkit_SOURCES = interop.c
interop_nbdkit_CPPFLAGS = \
	-I$(top_srcdir)/include \
	-DSERVER=\"$(NBDKIT)\" \
	-DSERVER_PARAMS='"-s", "--exit-with-parent", "file", tmpfile' \
	$(NULL)
interop_nbdkit_CFLAGS = $(WARNINGS_CFLAGS)
interop_nbdkit_LDADD = $(top_builddir)/lib/libnbd.la

interop_nbdkit_tls_certs_SOURCES = interop.c
interop_nbdkit_tls_certs_CPPFLAGS = \
	-I$(top_srcdir)/include \
	-DSERVER=\"$(NBDKIT)\" \
	-DSERVER_PARAMS='"--tls=require", "--tls-certificates=../tests/pki", "-s", "--exit-with-parent", "file", tmpfile' \
	-DCERTS=1 \
	-DTLS_MODE=LIBNBD_TLS_REQUIRE \
	$(NULL)
interop_nbdkit_tls_certs_CFLAGS = $(WARNINGS_CFLAGS)
interop_nbdkit_tls_certs_LDADD = $(top_builddir)/lib/libnbd.la

interop_nbdkit_tls_certs_allow_enabled_SOURCES = interop.c
interop_nbdkit_tls_certs_allow_enabled_CPPFLAGS = \
	-I$(top_srcdir)/include \
	-DSERVER=\"$(NBDKIT)\" \
	-DSERVER_PARAMS='"--tls=require", "--tls-certificates=../tests/pki", "-s", "--exit-with-parent", "file", tmpfile' \
	-DCERTS=1 \
	-DTLS_MODE=LIBNBD_TLS_ALLOW \
	$(NULL)
interop_nbdkit_tls_certs_allow_enabled_CFLAGS = $(WARNINGS_CFLAGS)
interop_nbdkit_tls_certs_allow_enabled_LDADD = $(top_builddir)/lib/libnbd.la

interop_nbdkit_tls_certs_allow_fallback_SOURCES = interop.c
interop_nbdkit_tls_certs_allow_fallback_CPPFLAGS = \
	-I$(top_srcdir)/include \
	-DSERVER=\"$(NBDKIT)\" \
	-DSERVER_PARAMS='"--tls=off", "-s", "--exit-with-parent", "file", tmpfile' \
	-DCERTS=1 \
	-DTLS_MODE=LIBNBD_TLS_ALLOW \
	-DTLS_FALLBACK=1 \
	$(NULL)
interop_nbdkit_tls_certs_allow_fallback_CFLAGS = $(WARNINGS_CFLAGS)
interop_nbdkit_tls_certs_allow_fallback_LDADD = $(top_builddir)/lib/libnbd.la

interop_nbdkit_tls_psk_SOURCES = interop.c
interop_nbdkit_tls_psk_CPPFLAGS = \
	-I$(top_srcdir)/include \
	-DSERVER=\"$(NBDKIT)\" \
	-DSERVER_PARAMS='"--tls=require", "--tls-psk=../tests/keys.psk", "-s", "--exit-with-parent", "file", tmpfile' \
	-DPSK=1 \
	-DTLS_MODE=LIBNBD_TLS_REQUIRE \
	$(NULL)
interop_nbdkit_tls_psk_CFLAGS = $(WARNINGS_CFLAGS)
interop_nbdkit_tls_psk_LDADD = $(top_builddir)/lib/libnbd.la

interop_nbdkit_tls_psk_allow_enabled_SOURCES = interop.c
interop_nbdkit_tls_psk_allow_enabled_CPPFLAGS = \
	-I$(top_srcdir)/include \
	-DSERVER=\"$(NBDKIT)\" \
	-DSERVER_PARAMS='"--tls=require", "--tls-psk=../tests/keys.psk", "-s", "--exit-with-parent", "file", tmpfile' \
	-DPSK=1 \
	-DTLS_MODE=LIBNBD_TLS_ALLOW \
	$(NULL)
interop_nbdkit_tls_psk_allow_enabled_CFLAGS = $(WARNINGS_CFLAGS)
interop_nbdkit_tls_psk_allow_enabled_LDADD = $(top_builddir)/lib/libnbd.la

interop_nbdkit_tls_psk_allow_fallback_SOURCES = interop.c
interop_nbdkit_tls_psk_allow_fallback_CPPFLAGS = \
	-I$(top_srcdir)/include \
	-DSERVER=\"$(NBDKIT)\" \
	-DSERVER_PARAMS='"--tls=off", "-s", "--exit-with-parent", "file", tmpfile' \
	-DPSK=1 \
	-DTLS_MODE=LIBNBD_TLS_ALLOW \
	-DTLS_FALLBACK=1 \
	$(NULL)
interop_nbdkit_tls_psk_allow_fallback_CFLAGS = $(WARNINGS_CFLAGS)
interop_nbdkit_tls_psk_allow_fallback_LDADD = $(top_builddir)/lib/libnbd.la

socket_activation_nbdkit_SOURCES = socket-activation.c
socket_activation_nbdkit_CPPFLAGS = \
	-I$(top_srcdir)/include \
	-DSERVER=\"$(NBDKIT)\" \
	-DSERVER_PARAMS='"file", tmpfile' \
	$(NULL)
socket_activation_nbdkit_CFLAGS = $(WARNINGS_CFLAGS)
socket_activation_nbdkit_LDADD = $(top_builddir)/lib/libnbd.la

endif HAVE_NBDKIT

check-valgrind:
	LIBNBD_VALGRIND=1 $(MAKE) check
