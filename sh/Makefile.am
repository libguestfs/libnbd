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
	nbdsh.pod \
	examples/LICENSE-FOR-EXAMPLES \
	examples/hexdump.sh \
	test-context.sh \
	test-error.sh \
	test-help.sh \
	test-long-options.sh \
	test-implicit-handle.sh \
	test-pattern.sh \
	test-short-options.sh \
	test-verbose.sh \
	test-version.sh \
	$(NULL)

if HAVE_PYTHON

bin_SCRIPTS = nbdsh

if HAVE_POD

man_MANS = nbdsh.1
CLEANFILES += $(man_MANS)

nbdsh.1: nbdsh.pod $(top_builddir)/podwrapper.pl
	$(PODWRAPPER) --section=1 --man $@ \
	    --html $(top_builddir)/html/$@.html \
	    --verbatim $(srcdir)/examples/hexdump.sh:__EXAMPLES_HEXDUMP__ \
	    $<

endif HAVE_POD

TESTS_ENVIRONMENT = \
	LIBNBD_DEBUG=1 \
	NBDKIT_DEBUG=1 \
	$(MALLOC_CHECKS) \
	EXPECTED_VERSION=$(VERSION) \
	$(NULL)
LOG_COMPILER = $(top_builddir)/run
TESTS = \
	test-help.sh \
	test-version.sh \
	test-verbose.sh \
	test-short-options.sh \
	test-long-options.sh \
	test-implicit-handle.sh \
	test-error.sh \
	$(NULL)

if HAVE_NBDKIT

TESTS += \
	test-context.sh \
	test-pattern.sh \
	$(NULL)

endif HAVE_NBDKIT

endif HAVE_PYTHON
