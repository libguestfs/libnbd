# nbd client library in userspace
# Copyright (C) 2013-2022 Red Hat Inc.
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

ML_TESTS = \
	test_010_import.ml \
	test_100_handle.ml \
	test_105_with_handle.ml \
	test_110_defaults.ml \
	test_120_set_non_defaults.ml \
	test_130_private_data.ml \
	test_140_explicit_close.ml \
	test_200_connect_command.ml \
	test_210_opt_abort.ml \
	test_220_opt_list.ml \
	test_230_opt_info.ml \
	test_240_opt_list_meta.ml \
	test_245_opt_list_meta_queries.ml \
	test_250_opt_set_meta.ml \
	test_255_opt_set_meta_queries.ml \
	test_300_get_size.ml \
	test_400_pread.ml \
	test_405_pread_structured.ml \
	test_410_pwrite.ml \
	test_460_block_status.ml \
	test_500_aio_pread.ml \
	test_505_aio_pread_structured_callback.ml \
	test_510_aio_pwrite.ml \
	test_580_aio_connect.ml \
	test_590_aio_copy.ml \
	test_600_debug_callback.ml \
	test_610_exception.ml \
	test_620_stats.ml \
	$(NULL)

EXTRA_DIST = $(ML_TESTS)

if HAVE_OCAML
if HAVE_NBDKIT

tests_bc = $(ML_TESTS:.ml=.bc)
tests_opt = $(ML_TESTS:.ml=.opt)

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

TESTS_ENVIRONMENT = \
	LIBNBD_DEBUG=1 \
	$(MALLOC_CHECKS) \
	OCAMLRUNPARAM=b \
	$(NULL)
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
	$(OCAMLFIND) ocamldep -I .. $(srcdir)/*.ml > $@
-include .depend
