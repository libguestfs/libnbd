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
	README \
	testcase_dir/newstyle \
	testcase_dir/newstyle-commands \
	testcase_dir/oldstyle \
	$(NULL)

noinst_PROGRAMS = libnbd-fuzz-wrapper
if ENABLE_LIBFUZZER
noinst_PROGRAMS += libnbd-libfuzzer-test
endif

libnbd_fuzz_wrapper_SOURCES = libnbd-fuzz-wrapper.c
libnbd_fuzz_wrapper_CPPFLAGS = -I$(top_srcdir)/include
libnbd_fuzz_wrapper_CFLAGS = $(WARNINGS_CFLAGS)
libnbd_fuzz_wrapper_LDADD = $(top_builddir)/lib/libnbd.la

libnbd_libfuzzer_test_SOURCES = libnbd-libfuzzer-test.c
libnbd_libfuzzer_test_CPPFLAGS = -I$(top_srcdir)/include
libnbd_libfuzzer_test_CFLAGS = $(WARNINGS_CFLAGS)
libnbd_libfuzzer_test_LDADD = $(top_builddir)/lib/libnbd.la
