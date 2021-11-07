#!/bin/bash -e
# nbd client library in userspace
# Copyright (C) 2021 Red Hat Inc.
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

# Create a distribution tree for serving the libnbd Go module.
#
# The web server must serve tree:
#
# libguestfs.org
# └── libnbd
#    ├── @latest
#    └── @v
#        ├── list
#        ├── v1.11.4.info
#        ├── v1.11.4.mod
#        └── v1.11.4.zip
#
# For example:
#
#   https://download.libguestfs.org/libnbd-golang/libguestfs.org/libnbd/@v/list
#
# To test the web server use:
#
#   GOPROXY=https://download.libguestfs.org go get libguestfs.org/libnbd
#
# This should download the module and add it to the local cache, by
# default in:
#
#   ~/go/pkg/mod/libguestfs.org/libnbd@v1.11.4/
#
# The Go tools will find the web server by looking at:
#
#   https://libguestfs.org/libnbd
#
# Which must serve html document with this meta tag:
#
#   <meta name="go-import" content="libguestfs.org/libnbd mod https://download.libguestfs.org/libnbd-golang">
#
# With this, users can consume the libnbd Go bindings using:
#
#   go get libguestfs.org/libnbd
#

version=$(git describe)

# Create module zip file
#
# libguestfs.org
# └── libnbd@v1.11.4
#    ├── go.mod
#    ├── handle.go
#    ...
#
# See https://golang.org/ref/mod#zip-files

version_dir="libguestfs.org/libnbd@$version"

mkdir -p $version_dir

tar cf - \
    --exclude examples \
    --exclude configure \
    --exclude "*_test.go" \
    go.mod README.md LICENSE *.go *.h |
    tar xf - -C $version_dir

zip -rq $version.zip $version_dir/*

rm -rf libguestfs.org

# Create web server tree.
#
# libguestfs.org
# └── libnbd
#    ├── @latest
#    └── @v
#        ├── list
#        ├── v1.11.4.info
#        ├── v1.11.4.mod
#        └── v1.11.4.zip
#
# See https://golang.org/ref/mod#serving-from-proxy

module_dir=libguestfs.org/libnbd
v_dir=$module_dir/@v

mkdir -p $v_dir

echo "{\"Version\": \"$version\"}" > $module_dir/@latest
echo "{\"Version\": \"$version\"}" > $v_dir/$version.info

# This is not entirely correct. This file should have a list of all
# versions available, here we create only single version. This should
# really be done on the server by appending the new version to the
# list file.
echo $version > $v_dir/list

cp go.mod $v_dir/$version.mod
mv $version.zip $v_dir

# Create tarball to upload and extract on the webserver. It should be
# extracted in the directory pointed by the "go-import" meta tag.
output=$PWD/libnbd-golang-$version.tar.gz
tar czf $output libguestfs.org

rm -rf libguestfs.org

echo output written to $output