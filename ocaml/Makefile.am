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

generator_built = \
	NBD.mli \
	NBD.ml \
	nbd-c.c

EXTRA_DIST = \
	$(generator_built)

CLEANFILES += *.annot *.cmi *.cmo *.cmx *.o *.a *.so

if HAVE_OCAML

OCAMLFLAGS = $(OCAML_FLAGS) $(OCAML_WARN_ERROR) -ccopt '$(CFLAGS)'
OCAMLPACKAGES = -package unix

noinst_DATA = mlnbd.cma META
if HAVE_OCAMLOPT
noinst_DATA += mlnbd.cmxa
endif

# Build the C part into a library, so that automake handles the C
# compilation step for us.  Note that we don't directly use this
# library; we link with the object files that it generates.
noinst_LIBRARIES = libnbdocaml.a

# Both calls to ocamlmklib below will create 'libmlnbd.a' and if run
# at the same time, they will stomp on or corrupt each others copy.
# Hence we have to serialize the calls.

CLEANFILES += stamp-mlnbd

mlnbd.cma mlnbd.cmxa: stamp-mlnbd

nbd_cmm = NBD.cmo
if HAVE_OCAMLOPT
nbd_cmm += NBD.cmx
endif

stamp-mlnbd: libnbdocaml.a $(nbd_cmm)
	$(OCAMLMKLIB) -o mlnbd \
	  -ldopt '$(LDFLAGS)' \
	  $(libnbdocaml_a_OBJECTS) NBD.cmo \
	-L../lib/.libs -lnbd
if HAVE_OCAMLOPT
	$(OCAMLMKLIB) -o mlnbd \
	  -ldopt '$(LDFLAGS)' \
	  $(libnbdocaml_a_OBJECTS) NBD.cmx \
	-L../lib/.libs -lnbd
endif
	touch $@

libnbdocaml_a_CPPFLAGS = \
	-I$(top_builddir) -I$(OCAMLLIB) -I$(top_srcdir)/ocaml \
	-I$(top_srcdir)/include \
	-DCAML_NAME_SPACE

libnbdocaml_a_CFLAGS = \
	$(WARNINGS_CFLAGS) \
	-fPIC

libnbdocaml_a_SOURCES = \
	nbd-c.c \
	nbd-c.h \
	buffer.c \
	handle.c \
	helpers.c

%.bc: %.cmo mlnbd.cma
	$(top_builddir)/libtool -dlopen $(top_builddir)/lib/.libs/libnbd.la --mode=execute \
        $(OCAMLFIND) ocamlc $(OCAMLFLAGS) -I . -package unix -linkpkg mlnbd.cma $< -o $@

if HAVE_OCAMLOPT
%.opt: %.cmx mlnbd.cmxa
	$(OCAMLFIND) ocamlopt $(OCAMLFLAGS) -cclib -L$(top_builddir)/lib/.libs -I . -package unix -linkpkg mlnbd.cmxa $< -o $@
endif

endif HAVE_OCAML

# Dependencies.
.depend: $(srcdir)/*.mli $(srcdir)/*.ml
	$(OCAMLFIND) ocamldep $^ > $@
-include .depend
