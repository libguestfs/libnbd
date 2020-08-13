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
	info-list.sh \
	info-list-json.sh \
	info-json.sh \
	info-oldstyle.sh \
	info-size.sh \
	info-text.sh \
	nbdinfo.pod \
	$(NULL)

TESTS_ENVIRONMENT = LIBNBD_DEBUG=1
LOG_COMPILER = $(top_builddir)/run
TESTS =

bin_PROGRAMS = nbdinfo

nbdinfo_SOURCES = nbdinfo.c
nbdinfo_CPPFLAGS = -I$(top_srcdir)/include
nbdinfo_CFLAGS = \
	$(WARNINGS_CFLAGS) \
	$(NULL)
nbdinfo_LDADD = \
	$(top_builddir)/lib/libnbd.la \
	$(NULL)

if HAVE_POD

man_MANS = \
	nbdinfo.1 \
	$(NULL)

nbdinfo.1: nbdinfo.pod $(top_builddir)/podwrapper.pl
	$(PODWRAPPER) --section=1 --man $@ \
	    --html $(top_builddir)/html/$@.html \
	    $<

endif HAVE_POD

if HAVE_LIBXML2

TESTS += \
	info-list.sh \
	info-list-json.sh \
	info-json.sh \
	info-oldstyle.sh \
	info-size.sh \
	info-text.sh \
	info-description.sh \
	$(NULL)

check-valgrind:
	LIBNBD_VALGRIND=1 $(MAKE) check

endif HAVE_LIBXML2
