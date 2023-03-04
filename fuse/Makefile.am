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
	nbdfuse.pod \
	test-errors.sh \
	test-file-mode.sh \
	test-long-options.sh \
	test-nbdkit-command.sh \
	test-nbdkit-file-null.sh \
	test-nbdkit.sh \
	test-parallel.sh \
	test-pattern.sh \
	test-qcow2.sh \
	test-short-options.sh \
	test-trim.sh \
	test-unix.sh \
	test-version.sh \
	test-zero.sh \
	$(NULL)

TESTS_ENVIRONMENT = \
	LIBNBD_DEBUG=1 \
	$(MALLOC_CHECKS) \
	EXPECTED_VERSION=$(VERSION) \
	$(NULL)
LOG_COMPILER = $(top_builddir)/run
TESTS =

if HAVE_FUSE

bin_PROGRAMS = nbdfuse

nbdfuse_SOURCES = \
	nbdfuse.c \
	nbdfuse.h \
	operations.c \
	$(NULL)
nbdfuse_CPPFLAGS = \
	-I$(top_srcdir)/include \
	-I$(top_srcdir)/common/include \
	-I$(top_srcdir)/common/utils \
	$(NULL)
nbdfuse_CFLAGS = $(WARNINGS_CFLAGS) $(FUSE_CFLAGS)
nbdfuse_LDADD = \
	$(top_builddir)/common/utils/libutils.la \
	$(top_builddir)/lib/libnbd.la \
	$(FUSE_LIBS) \
	$(NULL)

if HAVE_POD

man_MANS = \
	nbdfuse.1 \
	$(NULL)

nbdfuse.1: nbdfuse.pod $(top_builddir)/podwrapper.pl
	$(PODWRAPPER) --section=1 --man $@ \
	    --html $(top_builddir)/html/$@.html \
	    $<

endif HAVE_POD

TESTS += \
	test-long-options.sh \
	test-short-options.sh \
	test-version.sh \
	test-file-mode.sh \
	test-nbdkit.sh \
	test-nbdkit-command.sh \
	test-nbdkit-file-null.sh \
	test-pattern.sh \
	test-parallel.sh \
	test-unix.sh \
	test-qcow2.sh \
	test-trim.sh \
	test-zero.sh \
	test-errors.sh \
	$(NULL)

endif HAVE_FUSE

check-valgrind:
	LIBNBD_VALGRIND=1 $(MAKE) check
