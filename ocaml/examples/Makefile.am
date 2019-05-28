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

EXTRA_DIST = \
	extents.ml

CLEANFILES += *.annot *.cmi *.cmo *.cmx *.o *.a *.so *.bc *.opt

if HAVE_OCAML

OCAMLFLAGS = $(OCAML_FLAGS) $(OCAML_WARN_ERROR) -ccopt '$(CFLAGS)' -I ..
OCAMLPACKAGES = -package unix

noinst_SCRIPTS = extents.bc
if HAVE_OCAMLOPT
noinst_SCRIPTS += extents.opt
endif

%.bc: %.cmo ../mlnbd.cma
	LD_LIBRARY_PATH=../../lib/.libs $(OCAMLFIND) ocamlc $(OCAMLFLAGS) -package unix -linkpkg mlnbd.cma $< -o $@

if HAVE_OCAMLOPT
%.opt: %.cmx ../mlnbd.cmxa
	LD_LIBRARY_PATH=../../lib/.libs $(OCAMLFIND) ocamlopt $(OCAMLFLAGS) -cclib -L$(top_builddir)/lib/.libs -I . -package unix -linkpkg mlnbd.cmxa $< -o $@
endif

endif HAVE_OCAML

# Dependencies.
#.depend: $(srcdir)/*.mli $(srcdir)/*.ml
.depend: $(srcdir)/*.ml
	$(OCAMLFIND) ocamldep -I .. $^ > $@
-include .depend
