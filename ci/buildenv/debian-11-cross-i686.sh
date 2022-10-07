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
            diffutils \
            flake8 \
            fuse3 \
            git \
            gnutls-bin \
            golang \
            iproute2 \
            jq \
            libtool-bin \
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
    export DEBIAN_FRONTEND=noninteractive
    dpkg --add-architecture i386
    apt-get update
    apt-get dist-upgrade -y
    apt-get install --no-install-recommends -y dpkg-dev
    apt-get install --no-install-recommends -y \
            g++-i686-linux-gnu \
            gcc-i686-linux-gnu \
            libc6-dev:i386 \
            libev-dev:i386 \
            libfuse3-dev:i386 \
            libglib2.0-dev:i386 \
            libgnutls28-dev:i386 \
            libxml2-dev:i386
    mkdir -p /usr/local/share/meson/cross
    echo "[binaries]\n\
c = '/usr/bin/i686-linux-gnu-gcc'\n\
ar = '/usr/bin/i686-linux-gnu-gcc-ar'\n\
strip = '/usr/bin/i686-linux-gnu-strip'\n\
pkgconfig = '/usr/bin/i686-linux-gnu-pkg-config'\n\
\n\
[host_machine]\n\
system = 'linux'\n\
cpu_family = 'x86'\n\
cpu = 'i686'\n\
endian = 'little'" > /usr/local/share/meson/cross/i686-linux-gnu
    dpkg-query --showformat '${Package}_${Version}_${Architecture}\n' --show > /packages.txt
    mkdir -p /usr/libexec/ccache-wrappers
    ln -s /usr/bin/ccache /usr/libexec/ccache-wrappers/i686-linux-gnu-c++
    ln -s /usr/bin/ccache /usr/libexec/ccache-wrappers/i686-linux-gnu-cc
    ln -s /usr/bin/ccache /usr/libexec/ccache-wrappers/i686-linux-gnu-g++
    ln -s /usr/bin/ccache /usr/libexec/ccache-wrappers/i686-linux-gnu-gcc
}

export CCACHE_WRAPPERSDIR="/usr/libexec/ccache-wrappers"
export LANG="en_US.UTF-8"
export MAKE="/usr/bin/make"

export ABI="i686-linux-gnu"
export CONFIGURE_OPTS="--hosti686-linux-gnu"
