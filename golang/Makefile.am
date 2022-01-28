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

source_files = \
	aio_buffer.go \
	bindings.go \
	callbacks.go \
	closures.go \
	handle.go \
	wrappers.go \
	wrappers.h \
	libnbd_010_load_test.go \
	libnbd_020_aio_buffer_test.go \
	libnbd_100_handle_test.go \
	libnbd_110_defaults_test.go \
	libnbd_120_set_non_defaults_test.go \
	libnbd_200_connect_command_test.go \
	libnbd_210_opt_abort_test.go \
	libnbd_220_opt_list_test.go \
	libnbd_230_opt_info_test.go \
	libnbd_240_opt_list_meta_test.go \
	libnbd_300_get_size_test.go \
	libnbd_400_pread_test.go \
	libnbd_405_pread_structured_test.go \
	libnbd_410_pwrite_test.go \
	libnbd_460_block_status_test.go \
	libnbd_500_aio_pread_test.go \
	libnbd_510_aio_pwrite_test.go \
	libnbd_590_aio_copy_test.go \
	libnbd_600_debug_callback_test.go \
	libnbd_610_error_test.go \
	$(NULL)

generator_built = \
	bindings.go \
	closures.go \
	wrappers.go \
	wrappers.h \
	$(NULL)

EXTRA_DIST = \
	$(generator_built) \
	.gitignore \
	LICENSE \
	README.md \
	aio_buffer.go \
	callbacks.go \
	configure/go.mod \
	configure/test.go \
	go.mod \
	handle.go \
	libnbd-golang.pod \
	libnbd_*_test.go \
	make-dist.sh \
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
	$(abs_top_builddir)/run $(GOLANG) build

TESTS_ENVIRONMENT = \
	LIBNBD_DEBUG=1 \
	$(MALLOC_CHECKS) \
	abs_top_srcdir=$(abs_top_srcdir) \
	$(NULL)
LOG_COMPILER = $(top_builddir)/run
TESTS = run-tests.sh

endif

CLEANFILES += *~
