# nbd client library in userspace
# Copyright (C) 2013-2020 Red Hat Inc.
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

# http://golang.org/doc/code.html#Organization
pkg = libguestfs.org/libnbd

source_files = \
	src/$(pkg)/aio_buffer.go \
	src/$(pkg)/bindings.go \
	src/$(pkg)/callbacks.go \
	src/$(pkg)/closures.go \
	src/$(pkg)/handle.go \
	src/$(pkg)/wrappers.go \
	src/$(pkg)/wrappers.h \
	src/$(pkg)/libnbd_*_test.go

generator_built = \
	src/$(pkg)/bindings.go \
	src/$(pkg)/closures.go \
	src/$(pkg)/wrappers.go \
	src/$(pkg)/wrappers.h

EXTRA_DIST = \
	src/$(pkg)/.gitignore \
	src/$(pkg)/aio_buffer.go \
	src/$(pkg)/callbacks.go \
	src/$(pkg)/handle.go \
	src/$(pkg)/libnbd_*_test.go \
	$(generator_built) \
	config-test.go \
	libnbd-golang.pod \
	run-tests.sh

if HAVE_POD

man_MANS = libnbd-golang.3
CLEANFILES += $(man_MANS)

libnbd-golang.3: libnbd-golang.pod $(top_builddir)/podwrapper.pl
	$(PODWRAPPER) --section=3 --man $@ \
	    --html $(top_builddir)/html/$@.html \
	    $<

endif HAVE_POD

if HAVE_GOLANG

golangpkgdir = $(GOROOT)/pkg/$(GOOS)_$(GOARCH)/$(pkg)
golangsrcdir = $(GOROOT)/src/pkg/$(pkg)

golangpkg_DATA = \
	pkg/$(GOOS)_$(GOARCH)/$(pkg).a

pkg/$(GOOS)_$(GOARCH)/$(pkg).a: $(source_files)
	$(top_builddir)/run $(GOLANG) install $(pkg)

golangsrc_DATA = $(source_files)

TESTS_ENVIRONMENT = pkg=$(pkg) LIBNBD_DEBUG=1
LOG_COMPILER = $(top_builddir)/run
TESTS = run-tests.sh

endif

CLEANFILES += src/$(pkg)/*~

clean-local:
	rm -rf pkg
