# nbd client library in userspace
# Copyright (C) 2020 Red Hat Inc.
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
	copy-file-to-nbd.sh \
	copy-nbd-to-block.sh \
	copy-nbd-to-file.sh \
	copy-nbd-to-nbd.sh \
	copy-nbd-to-small-block-error.sh \
	copy-nbd-to-small-nbd-error.sh \
	copy-nbd-to-stdout.sh \
	copy-stdin-to-nbd.sh \
	nbdcopy.pod \
	$(NULL)

TESTS_ENVIRONMENT = LIBNBD_DEBUG=1
LOG_COMPILER = $(top_builddir)/run
TESTS =

nbdcopy_SOURCES = \
	nbdcopy.c \
	nbdcopy.h \
	multi-thread-copying.c \
	progress.c \
	synch-copying.c \
	$(NULL)
nbdcopy_CPPFLAGS = \
	-I$(top_srcdir)/include \
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
	copy-file-to-nbd.sh \
	copy-nbd-to-file.sh \
	copy-nbd-to-nbd.sh \
	copy-nbd-to-small-nbd-error.sh \
	copy-stdin-to-nbd.sh \
	copy-nbd-to-stdout.sh \
	$(ROOT_TESTS) \
	$(NULL)

check-valgrind:
	LIBNBD_VALGRIND=1 $(MAKE) check

# Test which require root permissions.
#
# We have to run these tests serially (-j1) because they all try to
# use /dev/nbd0.
check-root:
	$(MAKE) check -j1 TESTS="$(ROOT_TESTS)"

endif HAVE_LIBXML2
