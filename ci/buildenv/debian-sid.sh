# THIS FILE WAS AUTO-GENERATED
#
#  $ lcitool manifest ci/manifest.yml
#
# https://gitlab.com/libvirt/libvirt-ci

function install_buildenv() {
    export DEBIAN_FRONTEND=noninteractive
    apt-get update
    apt-get dist-upgrade -y
    apt-get install --no-install-recommends -y \
            autoconf \
            automake \
            bash-completion \
            bsdextrautils \
            ca-certificates \
            ccache \
            clang \
            diffutils \
            flake8 \
            fuse3 \
            g++ \
            gcc \
            git \
            gnutls-bin \
            golang \
            iproute2 \
            jq \
            libc6-dev \
            libev-dev \
            libfuse3-dev \
            libglib2.0-dev \
            libgnutls28-dev \
            libtool-bin \
            libxml2-dev \
            locales \
            make \
            nbd-client \
            nbd-server \
            nbdkit \
            ocaml \
            ocaml-findlib \
            ocaml-nox \
            perl \
            perl-base \
            pkgconf \
            python3-dev \
            qemu-system \
            qemu-utils \
            sed \
            valgrind
    sed -Ei 's,^# (en_US\.UTF-8 .*)$,\1,' /etc/locale.gen
    dpkg-reconfigure locales
    dpkg-query --showformat '${Package}_${Version}_${Architecture}\n' --show > /packages.txt
    mkdir -p /usr/libexec/ccache-wrappers
    ln -s /usr/bin/ccache /usr/libexec/ccache-wrappers/c++
    ln -s /usr/bin/ccache /usr/libexec/ccache-wrappers/cc
    ln -s /usr/bin/ccache /usr/libexec/ccache-wrappers/clang
    ln -s /usr/bin/ccache /usr/libexec/ccache-wrappers/g++
    ln -s /usr/bin/ccache /usr/libexec/ccache-wrappers/gcc
}

export CCACHE_WRAPPERSDIR="/usr/libexec/ccache-wrappers"
export LANG="en_US.UTF-8"
export MAKE="/usr/bin/make"
