# THIS FILE WAS AUTO-GENERATED
#
#  $ lcitool manifest ci/manifest.yml
#
# https://gitlab.com/libvirt/libvirt-ci

FROM registry.opensuse.org/opensuse/leap:15.3

RUN zypper update -y && \
    zypper install -y \
           autoconf \
           automake \
           bash-completion \
           ca-certificates \
           ccache \
           clang \
           diffutils \
           fuse3 \
           fuse3-devel \
           gcc \
           gcc-c++ \
           git \
           glib2-devel \
           glibc-devel \
           glibc-locale \
           gnutls \
           go \
           iproute2 \
           jq \
           libev-devel \
           libgnutls-devel \
           libtool \
           libxml2-devel \
           make \
           nbd \
           nbdkit \
           ocaml \
           ocaml-findlib \
           ocaml-ocamldoc \
           perl \
           perl-Pod-Simple \
           perl-base \
           pkgconfig \
           python3-devel \
           python3-flake8 \
           qemu \
           qemu-tools \
           sed \
           util-linux \
           valgrind && \
    zypper clean --all && \
    rpm -qa | sort > /packages.txt && \
    mkdir -p /usr/libexec/ccache-wrappers && \
    ln -s /usr/bin/ccache /usr/libexec/ccache-wrappers/c++ && \
    ln -s /usr/bin/ccache /usr/libexec/ccache-wrappers/cc && \
    ln -s /usr/bin/ccache /usr/libexec/ccache-wrappers/clang && \
    ln -s /usr/bin/ccache /usr/libexec/ccache-wrappers/g++ && \
    ln -s /usr/bin/ccache /usr/libexec/ccache-wrappers/gcc

ENV LANG "en_US.UTF-8"
ENV MAKE "/usr/bin/make"
ENV CCACHE_WRAPPERSDIR "/usr/libexec/ccache-wrappers"
