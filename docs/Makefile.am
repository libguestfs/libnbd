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

api_built=

# Our use of sinclude to bypass Automake is intentional; this file won't
# exist in a fresh git checkout until after the generator has run, but
# should already be present in any released tarball.
sinclude $(srcdir)/Makefile.inc

generator_built = \
	Makefile.inc \
	$(api_built:%=%.pod) \
	$(NULL)

EXTRA_DIST = \
	$(generator_built) \
	libnbd.pod \
	nbd_create.pod \
	nbd_close.3 \
	nbd_get_error.3 \
	nbd_get_errno.3 \
	$(NULL)

if HAVE_POD

man_MANS = \
	libnbd.3 \
	nbd_create.3 \
	nbd_close.3 \
	nbd_get_error.3 \
	nbd_get_errno.3 \
	$(api_built:%=%.3) \
	$(NULL)
CLEANFILES += \
	libnbd.3 \
	nbd_create.3 \
	$(api_built:%=%.3) \
	$(NULL)

# Generic .pod -> .3
nbd_%.3: nbd_%.pod $(top_builddir)/podwrapper.pl
	$(PODWRAPPER) --allow-long-lines --section=3 --man $@ \
	    --html $(top_builddir)/html/$@.html \
	    $<

# Specific .pod -> .3, overridden to forbid long lines
libnbd.3: libnbd.pod $(top_builddir)/podwrapper.pl
	$(PODWRAPPER) --section=3 --man $@ \
	    --html $(top_builddir)/html/$@.html \
	    $<

nbd_create.3: nbd_create.pod $(top_builddir)/podwrapper.pl
	$(PODWRAPPER) --section=3 --man $@ \
	    --html $(top_builddir)/html/$@.html \
	    $<

endif HAVE_POD
