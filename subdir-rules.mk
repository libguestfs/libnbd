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

# subdir-rules.mk is included only in subdirectories.
# common-rules.mk is included in every Makefile.am.

include $(top_srcdir)/common-rules.mk

%.cmi: %.mli
	$(OCAMLFIND) ocamlc $(OCAMLFLAGS) $(OCAMLPACKAGES) -c $< -o $@
%.cmo: %.ml
	$(OCAMLFIND) ocamlc $(OCAMLFLAGS) $(OCAMLPACKAGES) -c $< -o $@
if HAVE_OCAMLOPT
%.cmx: %.ml
	$(OCAMLFIND) ocamlopt $(OCAMLFLAGS) $(OCAMLPACKAGES) -c $< -o $@
endif

$(top_builddir)/podwrapper.pl: $(top_srcdir)/podwrapper.pl.in
	$(MAKE) -C $(top_builddir) podwrapper.pl

# In tests, include $(MALLOC_CHECKS) in TESTS_ENVIRONMENT to find some
# use-after-free and uninitialized read problems when using glibc.
# This doesn't affect other libc.
random = $(shell bash -c 'echo $$(( 1 + (RANDOM & 255) ))')
if HAVE_GLIBC_234
MALLOC_CHECKS = \
	LD_PRELOAD="$${LD_PRELOAD:+"$$LD_PRELOAD:"}libc_malloc_debug.so.0" \
	GLIBC_TUNABLES=glibc.malloc.check=1:glibc.malloc.perturb=$(random) \
	$(NULL)
else
MALLOC_CHECKS = \
	MALLOC_CHECK_=1 \
	MALLOC_PERTURB_=$(random) \
	$(NULL)
endif
