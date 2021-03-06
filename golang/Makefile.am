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
	src/$(pkg)/libnbd_010_load_test.go \
	src/$(pkg)/libnbd_100_handle_test.go \
	src/$(pkg)/libnbd_110_defaults_test.go \
	src/$(pkg)/libnbd_120_set_non_defaults_test.go \
	src/$(pkg)/libnbd_200_connect_command_test.go \
	src/$(pkg)/libnbd_210_opt_abort_test.go \
	src/$(pkg)/libnbd_220_opt_list_test.go \
	src/$(pkg)/libnbd_230_opt_info_test.go \
	src/$(pkg)/libnbd_240_opt_list_meta_test.go \
	src/$(pkg)/libnbd_300_get_size_test.go \
	src/$(pkg)/libnbd_400_pread_test.go \
	src/$(pkg)/libnbd_405_pread_structured_test.go \
	src/$(pkg)/libnbd_410_pwrite_test.go \
	src/$(pkg)/libnbd_460_block_status_test.go \
	src/$(pkg)/libnbd_500_aio_pread_test.go \
	src/$(pkg)/libnbd_510_aio_pwrite_test.go \
	src/$(pkg)/libnbd_590_aio_copy_test.go \
	src/$(pkg)/libnbd_600_debug_callback_test.go \
	src/$(pkg)/libnbd_610_error_test.go \
	$(NULL)

generator_built = \
	src/$(pkg)/bindings.go \
	src/$(pkg)/closures.go \
	src/$(pkg)/wrappers.go \
	src/$(pkg)/wrappers.h \
	$(NULL)

EXTRA_DIST = \
	src/$(pkg)/.gitignore \
	src/$(pkg)/go.mod \
	src/$(pkg)/aio_buffer.go \
	src/$(pkg)/callbacks.go \
	src/$(pkg)/handle.go \
	src/$(pkg)/libnbd_*_test.go \
	$(generator_built) \
	config-test.go \
	libnbd-golang.pod \
	run-tests.sh \
	$(NULL)

if HAVE_POD

man_MANS = libnbd-golang.3
CLEANFILES += $(man_MANS)

libnbd-golang.3: libnbd-golang.pod $(top_builddir)/podwrapper.pl
	$(PODWRAPPER) --section=3 --man $@ \
	    --html $(top_builddir)/html/$@.html \
	    $<

endif HAVE_POD

if HAVE_GOLANG

all-local: $(source_files)
	cd src/$(pkg) && \
	$(abs_top_builddir)/run $(GOLANG) build

TESTS_ENVIRONMENT = \
	LIBNBD_DEBUG=1 \
	MALLOC_CHECK_=1 \
	MALLOC_PERTURB_=$(shell bash -c 'echo $$(( 1 + (RANDOM & 255) ))') \
	pkg=$(pkg) \
	abs_top_srcdir=$(abs_top_srcdir) \
	$(NULL)
LOG_COMPILER = $(top_builddir)/run
TESTS = run-tests.sh

endif

CLEANFILES += src/$(pkg)/*~
