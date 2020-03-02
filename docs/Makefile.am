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
# should already be present in any released tarball.  But, since automake
# can't see into this file, it did not hoist the resulting prerequisites
# prior to its 'all-am: Makefile $(MANS)' rule, which gets parsed before
# $(MANS) has grown in size, so we have to add a second all-am dependency.
sinclude $(srcdir)/Makefile.inc
all-am: $(api_built:%=%.3)

generator_built = \
	Makefile.inc \
	api-links.pod \
	api-flag-links.pod \
	$(api_built:%=%.pod) \
	$(NULL)

EXTRA_DIST = \
	$(generator_built) \
	libnbd.pod \
	libnbd-release-notes-1.2.pod \
	libnbd-release-notes-1.4.pod \
	libnbd-security.pod \
	nbd_create.pod \
	nbd_close.3 \
	nbd_get_error.3 \
	nbd_get_errno.3 \
	$(NULL)

if HAVE_POD

man_MANS = \
	libnbd.3 \
	libnbd-release-notes-1.2.1 \
	libnbd-release-notes-1.4.1 \
	libnbd-security.3 \
	nbd_create.3 \
	nbd_close.3 \
	nbd_get_error.3 \
	nbd_get_errno.3 \
	$(api_built:%=%.3) \
	$(NULL)
CLEANFILES += \
	libnbd.3 \
	libnbd-release-notes-1.2.1 \
	libnbd-release-notes-1.4.1 \
	libnbd-security.3 \
	nbd_create.3 \
	$(api_built:%=%.3) \
	$(NULL)

nbd_%.3: nbd_%.pod $(top_builddir)/podwrapper.pl
	$(PODWRAPPER) --section=3 --man $@ \
	    --html $(top_builddir)/html/$@.html \
	    $<

libnbd.3: libnbd.pod $(top_builddir)/podwrapper.pl \
		api-links.pod api-flag-links.pod
	$(PODWRAPPER) --section=3 --man $@ \
	    --insert $(srcdir)/api-links.pod:__API_LINKS__ \
	    --insert $(srcdir)/api-flag-links.pod:__API_FLAG_LINKS__ \
	    --html $(top_builddir)/html/$@.html \
	    $<

libnbd-release-notes-1.2.1: libnbd-release-notes-1.2.pod
	$(PODWRAPPER) --section=1 --man $@ \
	    --html $(top_builddir)/html/$@.html \
	    $<

libnbd-release-notes-1.4.1: libnbd-release-notes-1.4.pod
	$(PODWRAPPER) --section=1 --man $@ \
	    --html $(top_builddir)/html/$@.html \
	    $<

libnbd-security.3: libnbd-security.pod
	$(PODWRAPPER) --section=3 --man $@ \
	    --html $(top_builddir)/html/$@.html \
	    $<

endif HAVE_POD
