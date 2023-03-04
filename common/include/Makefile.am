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
	ansi-colours.h \
	array-size.h \
	byte-swapping.h \
	checked-overflow.h \
	compiler-macros.h \
	isaligned.h \
	ispowerof2.h \
	iszero.h \
	minmax.h \
	rounding.h \
	static-assert.h \
	unique-name.h \
	$(NULL)

# Unit tests.

TESTS = \
	test-array-size \
	test-byte-swapping \
	test-checked-overflow \
	test-isaligned \
	test-ispowerof2 \
	test-iszero \
	test-minmax \
	$(NULL)
check_PROGRAMS = $(TESTS)

test_array_size_SOURCES = test-array-size.c array-size.h
test_array_size_CPPFLAGS = -I$(srcdir)
test_array_size_CFLAGS = $(WARNINGS_CFLAGS)

test_byte_swapping_SOURCES = test-byte-swapping.c byte-swapping.h
test_byte_swapping_CPPFLAGS = -I$(srcdir)
test_byte_swapping_CFLAGS = $(WARNINGS_CFLAGS)

test_checked_overflow_SOURCES = test-checked-overflow.c checked-overflow.h
test_checked_overflow_CPPFLAGS = -I$(srcdir)
test_checked_overflow_CFLAGS = $(WARNINGS_CFLAGS)

test_isaligned_SOURCES = test-isaligned.c isaligned.h
test_isaligned_CPPFLAGS = -I$(srcdir)
test_isaligned_CFLAGS = $(WARNINGS_CFLAGS)

test_ispowerof2_SOURCES = test-ispowerof2.c ispowerof2.h
test_ispowerof2_CPPFLAGS = -I$(srcdir)
test_ispowerof2_CFLAGS = $(WARNINGS_CFLAGS)

test_iszero_SOURCES = test-iszero.c iszero.h
test_iszero_CPPFLAGS = -I$(srcdir)
test_iszero_CFLAGS = $(WARNINGS_CFLAGS)

test_minmax_SOURCES = test-minmax.c minmax.h
test_minmax_CPPFLAGS = -I$(srcdir)
test_minmax_CFLAGS = $(WARNINGS_CFLAGS)
