# nbd client library in userspace
# Copyright (C) 2013-2021 Red Hat Inc.
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
	array-size.h \
	byte-swapping.h \
	checked-overflow.h \
	compiler-macros.h \
	isaligned.h \
	ispowerof2.h \
	iszero.h \
	minmax.h \
	rounding.h \
	unique-name.h \
	$(NULL)

# Unit tests.

TESTS = \
	test-array-size \
	test-checked-overflow \
	$(NULL)
check_PROGRAMS = $(TESTS)

test_array_size_SOURCES = test-array-size.c array-size.h
test_array_size_CPPFLAGS = -I$(srcdir)
test_array_size_CFLAGS = $(WARNINGS_CFLAGS)

test_checked_overflow_SOURCES = test-checked-overflow.c checked-overflow.h
test_checked_overflow_CPPFLAGS = -I$(srcdir)
test_checked_overflow_CFLAGS = $(WARNINGS_CFLAGS)
