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

CLEANFILES += *.pyc t/*~

clean-local:
	-rm -rf __pycache__

generator_built = \
	libnbdmod.c \
	methods.c \
	methods.h \
	nbd.py

EXTRA_DIST = \
	$(generator_built) \
	nbdsh.py \
	$(srcdir)/t/*.py

if HAVE_PYTHON

pythondir = $(PYTHON_INSTALLDIR)
python_DATA = nbd.py nbdsh.py

python_LTLIBRARIES = libnbdmod.la

BUILT_SOURCES = $(generator_built)

libnbdmod_la_SOURCES = \
	handle.c \
	libnbdmod.c \
	methods.c \
	methods.h \
	utils.c
libnbdmod_la_CPPFLAGS = \
	$(PYTHON_CFLAGS) \
	-I$(top_srcdir)/include \
	-I$(top_srcdir)/common/include
libnbdmod_la_CFLAGS = \
	$(WARNINGS_CFLAGS)
libnbdmod_la_LIBADD = \
	$(top_builddir)/lib/libnbd.la
libnbdmod_la_LDFLAGS = \
	-avoid-version -shared -module -shrext $(PYTHON_EXT_SUFFIX)

if HAVE_NBDKIT

TESTS_ENVIRONMENT = LIBNBD_DEBUG=1
LOG_COMPILER = $(top_builddir)/run
TESTS = run-python-tests

endif HAVE_NBDKIT

endif HAVE_PYTHON
