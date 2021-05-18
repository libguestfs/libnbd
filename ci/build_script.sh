#!/bin/sh

set -e

main() {
    autoreconf -if

    CONFIG_ARGS="\
--enable-gcc-warnings \
--enable-fuse \
--enable-ocaml \
--enable-python \
--enable-golang \
--with-gnutls \
--with-libxml2 \
"

    if test "$GOLANG" != "skip"
    then
       CONFIG_ARGS="$CONFIG_ARGS --enable-golang"
    fi

    ./configure $CONFIG_ARGS

    $MAKE

    if test -n "$CROSS"
    then
       return 0
    fi

    $MAKE check

    if test "$CHECK_VALGRIND" = "force"
    then
        $MAKE check-valgrind
    fi

    if test "$DIST" != "skip"
    then
        $MAKE dist
        $MAKE maintainer-check-extra-dist
    fi

    if test "$DISTCHECK" = "force"
    then
        $MAKE distcheck
    fi
}

main "$@"
