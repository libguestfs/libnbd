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

include $(top_srcdir)/common-rules.mk

ACLOCAL_AMFLAGS = -I m4

EXTRA_DIST = \
	.dir-locals.el \
	.gitignore \
	html/pod.css \
	scripts/git.orderfile \
	SECURITY \
	$(NULL)

SUBDIRS = \
	generator \
	include \
	common/include \
	lib \
	docs \
	examples \
	valgrind \
	. \
	tests \
	python \
	sh \
	fuse \
	ocaml \
	ocaml/examples \
	ocaml/tests \
	golang \
	golang/examples \
	interop \
	fuzzing \
	bash \
	$(NULL)

noinst_SCRIPTS = run

# Check no files are missing from EXTRA_DIST rules, and that all
# generated files have been included in the tarball.  (Note you must
# have done 'make dist')
maintainer-check-extra-dist:
	@zcat $(PACKAGE_NAME)-$(VERSION).tar.gz | tar tf - | sort | \
	    sed 's,^$(PACKAGE_NAME)-$(VERSION)/,,' > tarfiles
	@git ls-files | \
	    sort > gitfiles
	@comm -13 tarfiles gitfiles > comm-out
	@echo Checking for differences between EXTRA_DIST and git ...
	@cat comm-out
	@[ ! -s comm-out ]
	@rm tarfiles gitfiles comm-out
	@echo PASS: EXTRA_DIST tests

check-valgrind: all
	@for d in tests fuse ocaml/tests interop; do \
	    $(MAKE) -C $$d check-valgrind || exit 1; \
	done

#----------------------------------------------------------------------
# Maintainers only!

# Commit everything in the current directory and set the commit
# message to the current version number.
maintainer-commit:
	git commit -a -m "Version $(VERSION)."

# Tag HEAD with the current version.
maintainer-tag:
	git tag -a v$(VERSION) -m "Version $(VERSION)." -f
