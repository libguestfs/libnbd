# THIS FILE WAS AUTO-GENERATED
#
#  $ lcitool manifest ci/manifest.yml
#
# https://gitlab.com/libvirt/libvirt-ci

FROM docker.io/library/alpine:3.14

RUN apk update && \
    apk upgrade && \
    apk add \
        autoconf \
        automake \
        bash-completion \
        ca-certificates \
        ccache \
        clang \
        diffutils \
        fuse3 \
        fuse3-dev \
        g++ \
        gcc \
        git \
        glib-dev \
        gnutls-dev \
        gnutls-utils \
        go \
        hexdump \
        iproute2 \
        jq \
        libev-dev \
        libtool \
        libxml2-dev \
        make \
        musl-dev \
        nbd \
        nbd-client \
        ocaml \
        ocaml-findlib-dev \
        ocaml-ocamldoc \
        perl \
        pkgconf \
        py3-flake8 \
        python3-dev \
        qemu \
        qemu-img \
        sed \
        valgrind && \
    apk list | sort > /packages.txt && \
    mkdir -p /usr/libexec/ccache-wrappers && \
    ln -s /usr/bin/ccache /usr/libexec/ccache-wrappers/c++ && \
    ln -s /usr/bin/ccache /usr/libexec/ccache-wrappers/cc && \
    ln -s /usr/bin/ccache /usr/libexec/ccache-wrappers/clang && \
    ln -s /usr/bin/ccache /usr/libexec/ccache-wrappers/g++ && \
    ln -s /usr/bin/ccache /usr/libexec/ccache-wrappers/gcc

ENV LANG "en_US.UTF-8"
ENV MAKE "/usr/bin/make"
ENV CCACHE_WRAPPERSDIR "/usr/libexec/ccache-wrappers"
