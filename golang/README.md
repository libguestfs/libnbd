# Libnbd Go binding

This module provides access to Network Block Device (NBD) servers from
the Go programming language, using the libnbd library.

Please see the manual to learn how to use this module:
https://libguestfs.org/libnbd-golang.3.html

This is part of libnbd, please check the project at:
https://gitlab.com/nbdkit/libnbd

## How consume this module

The Go binding cannot be consumed yet using the Go tools, but you
extract it from the tarball.

1. Download a tarball

        wget https://download.libguestfs.org/libnbd/1.10-stable/libnbd-1.10.1.tar.gz

2. Extract the sources from the golang directory

        mkdir pkg/libnbd

        tar xvf libnbd-1.11.1.tar.gz \
            --directory pkg/libnbd \
            --strip 2 \
            --exclude "*_test.go" \
            --exclude "examples" \
            --exclude "configure" \
            libnbd-1.11.1/golang/{go.mod,README.md,LICENSE,*.go,*.h}

3. Edit your go mode file to use the local copy

        go mod edit -replace libguestfs.org/libnbd=./pkg/libnbd

4. Install the libnbd development package

        dnf install libnbd-devel

   The package may be named differently on your distro.

## License

The software is copyright © Red Hat Inc. and licensed under the GNU
Lesser General Public License version 2 or above (LGPLv2+).  See
the file `LICENSE` for details.
