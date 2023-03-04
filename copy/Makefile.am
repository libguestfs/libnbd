# nbd client library in userspace
# Copyright Red Hat
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
	copy-block-to-nbd.sh \
	copy-file-to-file.sh \
	copy-file-to-nbd.sh \
	copy-file-to-null.sh \
	copy-file-to-qcow2.sh \
	copy-file-to-qcow2-compressed.sh \
	copy-nbd-to-block.sh \
	copy-nbd-to-file.sh \
	copy-nbd-to-hexdump.sh \
	copy-nbd-to-nbd.sh \
	copy-nbd-to-nbd2.sh \
	copy-nbd-to-null.sh \
	copy-nbd-to-small-block-error.sh \
	copy-nbd-to-small-nbd-error.sh \
	copy-nbd-to-sparse-file.sh \
	copy-nbd-to-stdout.sh \
	copy-nbd-error.sh \
	copy-progress-bar.sh \
	copy-sparse.sh \
	copy-sparse-allocated.sh \
	copy-sparse-no-extents.sh \
	copy-sparse-request-size.sh \
	copy-sparse-to-stream.sh \
	copy-stdin-to-nbd.sh \
	copy-stdin-to-null.sh \
	copy-tls.sh \
	copy-zero-to-nbd.sh \
	copy-zero-to-null.sh \
	nbdcopy.pod \
	test-long-options.sh \
	test-short-options.sh \
	test-verbose.sh \
	test-version.sh \
	$(NULL)

TESTS_ENVIRONMENT = \
	LIBNBD_DEBUG=1 \
	$(MALLOC_CHECKS) \
	EXPECTED_VERSION=$(VERSION) \
	PSKTOOL=$(PSKTOOL) \
	QEMU_NBD=$(QEMU_NBD) \
	$(NULL)
LOG_COMPILER = $(top_builddir)/run
TESTS =

nbdcopy_SOURCES = \
	nbdcopy.h \
	file-ops.c \
	main.c \
	multi-thread-copying.c \
	nbd-ops.c \
	null-ops.c \
	pipe-ops.c \
	progress.c \
	synch-copying.c \
	$(NULL)
nbdcopy_CPPFLAGS = \
	-I$(top_srcdir)/include \
	-I$(top_srcdir)/common/include \
	-I$(top_srcdir)/common/utils \
	$(NULL)
nbdcopy_CFLAGS = \
	$(WARNINGS_CFLAGS) \
	$(PTHREAD_CFLAGS) \
	$(NULL)
nbdcopy_LDADD = \
	$(PTHREAD_LIBS) \
	$(top_builddir)/common/utils/libutils.la \
	$(top_builddir)/lib/libnbd.la \
	$(NULL)

# This program depends on URI support; no point in installing it otherwise
if HAVE_LIBXML2

bin_PROGRAMS = nbdcopy

if HAVE_POD

man_MANS = \
	nbdcopy.1 \
	$(NULL)

nbdcopy.1: nbdcopy.pod $(top_builddir)/podwrapper.pl
	$(PODWRAPPER) --section=1 --man $@ \
	    --html $(top_builddir)/html/$@.html \
	    $<

endif HAVE_POD

ROOT_TESTS = \
	copy-block-to-nbd.sh \
	copy-nbd-to-block.sh \
	copy-nbd-to-small-block-error.sh \
	$(NULL)
TESTS += \
	copy-file-to-file.sh \
	copy-file-to-nbd.sh \
	copy-file-to-null.sh \
	copy-nbd-to-file.sh \
	copy-nbd-to-hexdump.sh \
	copy-nbd-to-nbd.sh \
	copy-nbd-to-nbd2.sh \
	copy-nbd-to-null.sh \
	copy-nbd-to-small-nbd-error.sh \
	copy-nbd-to-sparse-file.sh \
	copy-stdin-to-nbd.sh \
	copy-stdin-to-null.sh \
	copy-nbd-to-stdout.sh \
	copy-nbd-error.sh \
	copy-progress-bar.sh \
	copy-sparse.sh \
	copy-sparse-allocated.sh \
	copy-sparse-no-extents.sh \
	copy-sparse-request-size.sh \
	copy-sparse-to-stream.sh \
	copy-zero-to-nbd.sh \
	copy-zero-to-null.sh \
	$(ROOT_TESTS) \
	test-long-options.sh \
	test-short-options.sh \
	test-verbose.sh \
	test-version.sh \
	$(NULL)

if HAVE_QEMU_NBD
TESTS += \
	copy-file-to-qcow2.sh \
	copy-file-to-qcow2-compressed.sh \
	$(NULL)
endif

if HAVE_GNUTLS
if HAVE_PSKTOOL
TESTS += copy-tls.sh
endif
endif

check-valgrind:
	LIBNBD_VALGRIND=1 $(MAKE) check

# Test which require root permissions.
#
# We have to run these tests serially (-j1) because they all try to
# use /dev/nbd0.
check-root:
	$(MAKE) check -j1 TESTS="$(ROOT_TESTS)"

endif HAVE_LIBXML2
