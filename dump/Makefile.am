# nbd client library in userspace
# Copyright (C) 2020-2022 Red Hat Inc.
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
	dump-data.sh \
	dump-empty-qcow2.sh \
	dump-pattern.sh \
	nbddump.pod \
	test-long-options.sh \
	test-short-options.sh \
	test-version.sh \
	$(NULL)

bin_PROGRAMS = nbddump

nbddump_SOURCES = \
	dump.c \
	$(NULL)
nbddump_CPPFLAGS = \
	-I$(top_srcdir)/include \
	-I$(top_srcdir)/common/include \
	-I$(top_srcdir)/common/utils \
	$(NULL)
nbddump_CFLAGS = \
	$(WARNINGS_CFLAGS) \
	$(NULL)
nbddump_LDADD = \
	$(top_builddir)/common/utils/libutils.la \
	$(top_builddir)/lib/libnbd.la \
	$(NULL)

if HAVE_POD

man_MANS = \
	nbddump.1 \
	$(NULL)

nbddump.1: nbddump.pod $(top_builddir)/podwrapper.pl
	$(PODWRAPPER) --section=1 --man $@ \
	    --html $(top_builddir)/html/$@.html \
	    $<

endif HAVE_POD

TESTS_ENVIRONMENT = \
	LIBNBD_DEBUG=1 \
	$(MALLOC_CHECKS) \
	EXPECTED_VERSION=$(VERSION) \
	QEMU_NBD=$(QEMU_NBD) \
	$(NULL)
LOG_COMPILER = $(top_builddir)/run

TESTS = \
	dump-data.sh \
	dump-empty-qcow2.sh \
	dump-pattern.sh \
	test-long-options.sh \
	test-short-options.sh \
	test-version.sh \
	$(NULL)

check-valgrind:
	LIBNBD_VALGRIND=1 $(MAKE) check
