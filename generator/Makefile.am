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

DISTCLEANFILES = stamp-generator

# Note: These are not source files.  They are parsed by the generator
# and output to lib/states.c.
states_code = \
	states.c \
	states-connect.c \
	states-issue-command.c \
	states-magic.c \
	states-newstyle-opt-export-name.c \
	states-newstyle-opt-go.c \
	states-newstyle-opt-set-meta-context.c \
	states-newstyle-opt-starttls.c \
	states-newstyle-opt-structured-reply.c \
	states-newstyle.c \
	states-oldstyle.c \
	states-reply-simple.c \
	states-reply-structured.c \
	states-reply.c

EXTRA_DIST = \
	generator \
	$(states_code)

stamp-generator: generator $(states_code)
#	We must run this from the source directory since these
#	files are distributed in the tarball.
	rm -f $@
	cd $(top_srcdir) && OCAMLRUNPARAM=b generator/generator
	touch $@
