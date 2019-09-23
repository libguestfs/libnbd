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

CLEANFILES += *.annot *.cmi *.cmo *.cmx *.o *.a *.so *.bc *.opt

EXTRA_DIST = \
	test_010_import.ml \
	test_100_handle.ml \
	test_200_connect_command.ml \
	test_300_get_size.ml \
	test_400_pread.ml \
	test_405_pread_structured.ml \
	test_410_pwrite.ml \
	test_460_block_status.ml \
	test_500_aio_pread.ml \
	test_505_aio_pread_structured_callback.ml \
	test_510_aio_pwrite.ml \
	test_590_aio_copy.ml \
	test_600_debug_callback.ml \
	test_610_exception.ml \
	$(NULL)

if HAVE_OCAML
if HAVE_NBDKIT

tests_bc = \
	test_010_import.bc \
	test_100_handle.bc \
	test_200_connect_command.bc \
	test_300_get_size.bc \
	test_400_pread.bc \
	test_405_pread_structured.bc \
	test_410_pwrite.bc \
	test_460_block_status.bc \
	test_500_aio_pread.bc \
	test_505_aio_pread_structured_callback.bc \
	test_510_aio_pwrite.bc \
	test_590_aio_copy.bc \
	test_600_debug_callback.bc \
	test_610_exception.bc \
	$(NULL)

tests_opt = \
	test_010_import.opt \
	test_100_handle.opt \
	test_200_connect_command.opt \
	test_300_get_size.opt \
	test_400_pread.opt \
	test_405_pread_structured.opt \
	test_410_pwrite.opt \
	test_460_block_status.opt \
	test_500_aio_pread.opt \
	test_505_aio_pread_structured_callback.opt \
	test_510_aio_pwrite.opt \
	test_590_aio_copy.opt \
	test_600_debug_callback.opt \
	test_610_exception.opt \
	$(NULL)

OCAMLFLAGS = $(OCAML_FLAGS) $(OCAML_WARN_ERROR) -ccopt '$(CFLAGS)'
OCAMLPACKAGES = \
	-package unix \
	-I $(srcdir)/.. -I $(builddir)/.. -I $(srcdir) -I $(builddir)

check_SCRIPTS = $(tests_bc)
if HAVE_OCAMLOPT
check_SCRIPTS += $(tests_opt)
endif

%.bc: %.cmo ../mlnbd.cma
	LD_LIBRARY_PATH=../../lib/.libs \
	$(OCAMLFIND) ocamlc \
	  $(OCAMLFLAGS) $(OCAMLPACKAGES) -linkpkg \
	  mlnbd.cma $< -o $@

if HAVE_OCAMLOPT
%.opt: %.cmx ../mlnbd.cmxa
	LD_LIBRARY_PATH=../../lib/.libs \
	$(OCAMLFIND) ocamlopt \
	  $(OCAMLFLAGS) $(OCAMLPACKAGES) \
	  -cclib -L$(top_builddir)/lib/.libs -linkpkg \
	  mlnbd.cmxa $< -o $@
endif

TESTS_ENVIRONMENT = LIBNBD_DEBUG=1 OCAMLRUNPARAM=b
LOG_COMPILER = $(top_builddir)/run
TESTS = $(tests_bc)
if HAVE_OCAMLOPT
TESTS += $(tests_opt)
endif

endif HAVE_NBDKIT
endif HAVE_OCAML

check-valgrind:
	LIBNBD_VALGRIND=1 $(MAKE) check TESTS="$(tests_opt)"

# Dependencies.
#.depend: $(srcdir)/*.mli $(srcdir)/*.ml
.depend: $(srcdir)/*.ml
	$(OCAMLFIND) ocamldep -I .. $^ > $@
-include .depend
