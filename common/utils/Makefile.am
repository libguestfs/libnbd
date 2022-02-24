# nbdkit
# Copyright (C) 2019 Red Hat Inc.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
# * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#
# * Redistributions in binary form must reproduce the above copyright
# notice, this list of conditions and the following disclaimer in the
# documentation and/or other materials provided with the distribution.
#
# * Neither the name of Red Hat nor the names of its contributors may be
# used to endorse or promote products derived from this software without
# specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY RED HAT AND CONTRIBUTORS ''AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
# THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
# PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL RED HAT OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
# USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
# OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.

include $(top_srcdir)/common-rules.mk

noinst_LTLIBRARIES = libutils.la

libutils_la_SOURCES = \
	human-size.c \
	human-size.h \
	nbdkit-string.h \
	vector.c \
	vector.h \
	version.c \
	version.h \
	$(NULL)
libutils_la_CPPFLAGS = \
	-I$(top_srcdir)/include \
	-I$(top_srcdir)/common/include \
	$(NULL)
libutils_la_CFLAGS = \
	$(WARNINGS_CFLAGS) \
	$(NULL)
libutils_la_LIBADD = \
	$(NULL)

# Unit tests.

TESTS = test-human-size test-vector
check_PROGRAMS = test-human-size test-vector

test_human_size_SOURCES = test-human-size.c human-size.c human-size.h
test_human_size_CPPFLAGS = -I$(srcdir)
test_human_size_CFLAGS = $(WARNINGS_CFLAGS)

test_vector_SOURCES = test-vector.c vector.c vector.h bench.h
test_vector_CPPFLAGS = -I$(srcdir) -I$(top_srcdir)/common/include
test_vector_CFLAGS = $(WARNINGS_CFLAGS)

bench: test-vector
	LIBNBD_BENCH=1 ./test-vector
