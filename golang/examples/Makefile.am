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
	LICENSE-FOR-EXAMPLES \
	get_size/go.mod \
	get_size/get_size.go \
	read_first_sector/go.mod \
	read_first_sector/read_first_sector.go \
	simple_copy/go.mod \
	simple_copy/simple_copy.go \
	aio_copy/go.mod \
	aio_copy/aio_copy.go \
	$(NULL)

if HAVE_GOLANG

noinst_SCRIPTS = \
	get_size/get_size \
	read_first_sector/read_first_sector \
	simple_copy/simple_copy \
	aio_copy/aio_copy \
	$(NULL)

get_size/get_size: get_size/get_size.go
	cd get_size && \
	$(abs_top_builddir)/run go build -o get_size

read_first_sector/read_first_sector: read_first_sector/read_first_sector.go
	cd read_first_sector && \
	$(abs_top_builddir)/run go build -o read_first_sector

simple_copy/simple_copy: simple_copy/simple_copy.go
	cd simple_copy && \
	$(abs_top_builddir)/run go build -o simple_copy

aio_copy/aio_copy: aio_copy/aio_copy.go
	cd aio_copy && \
	$(abs_top_builddir)/run go build -o aio_copy

endif HAVE_GOLANG

CLEANFILES += \
	get_size/get_size \
	read_first_sector/read_first_sector \
	simple_copy/simple_copy \
	aio_copy/aio_copy \
	$(NULL)
