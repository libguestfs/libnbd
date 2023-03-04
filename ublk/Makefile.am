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
	nbdublk.pod \
	$(NULL)

TESTS_ENVIRONMENT = \
	LIBNBD_DEBUG=1 \
	$(MALLOC_CHECKS) \
	EXPECTED_VERSION=$(VERSION) \
	$(NULL)
LOG_COMPILER = $(top_builddir)/run
TESTS =

if HAVE_UBLK

bin_PROGRAMS = nbdublk

nbdublk_SOURCES = \
	nbdublk.c \
	nbdublk.h \
	tgt.c \
	not.cpp \
	$(NULL)
nbdublk_CPPFLAGS = \
	-I$(top_srcdir)/include \
	-I$(top_srcdir)/common/include \
	-I$(top_srcdir)/common/utils \
	$(NULL)
nbdublk_CFLAGS = $(WARNINGS_CFLAGS) $(UBLKSRV_CFLAGS)
nbdublk_CXXFLAGS = $(WARNINGS_CFLAGS) $(UBLKSRV_CFLAGS)
nbdublk_LDADD = \
	$(top_builddir)/common/utils/libutils.la \
	$(top_builddir)/lib/libnbd.la \
	$(UBLKSRV_LIBS) \
	$(NULL)

if HAVE_POD

man_MANS = \
	nbdublk.1 \
	$(NULL)

nbdublk.1: nbdublk.pod $(top_builddir)/podwrapper.pl
	$(PODWRAPPER) --section=1 --man $@ \
	    --html $(top_builddir)/html/$@.html \
	    $<

endif HAVE_POD

endif HAVE_UBLK
