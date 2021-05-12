# THIS FILE WAS AUTO-GENERATED
#
#  $ lcitool dockerfile opensuse-tumbleweed libnbd
#
# https://gitlab.com/libvirt/libvirt-ci/-/commit/5fd2cdeb8958df86a81b639fd096f626d0f5dbd0

FROM registry.opensuse.org/opensuse/tumbleweed:latest

RUN zypper update -y && \
    zypper install -y \
           autoconf \
           automake \
           bash-completion \
           ca-certificates \
           ccache \
           clang \
           coreutils \
           diffutils \
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
           perl-base \
           pkgconfig \
           python3-devel \
           python3-flake8 \
           qemu-kvm \
           qemu-tools \
           sed && \
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
