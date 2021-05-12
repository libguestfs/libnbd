# THIS FILE WAS AUTO-GENERATED
#
#  $ lcitool dockerfile --cross s390x debian-sid libnbd
#
# https://gitlab.com/libvirt/libvirt-ci/-/commit/5fd2cdeb8958df86a81b639fd096f626d0f5dbd0

FROM docker.io/library/debian:sid-slim

RUN export DEBIAN_FRONTEND=noninteractive && \
    apt-get update && \
    apt-get install -y eatmydata && \
    eatmydata apt-get dist-upgrade -y && \
    eatmydata apt-get install --no-install-recommends -y \
            autoconf \
            automake \
            bash-completion \
            ca-certificates \
            ccache \
            coreutils \
            diffutils \
            flake8 \
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
            perl-base \
            pkgconf \
            python3-dev \
            qemu-kvm \
            qemu-utils \
            sed && \
    eatmydata apt-get autoremove -y && \
    eatmydata apt-get autoclean -y && \
    sed -Ei 's,^# (en_US\.UTF-8 .*)$,\1,' /etc/locale.gen && \
    dpkg-reconfigure locales && \
    dpkg-query --showformat '${Package}_${Version}_${Architecture}\n' --show > /packages.txt && \
    mkdir -p /usr/libexec/ccache-wrappers && \
    ln -s /usr/bin/ccache /usr/libexec/ccache-wrappers/s390x-linux-gnu-c++ && \
    ln -s /usr/bin/ccache /usr/libexec/ccache-wrappers/s390x-linux-gnu-cc && \
    ln -s /usr/bin/ccache /usr/libexec/ccache-wrappers/s390x-linux-gnu-g++ && \
    ln -s /usr/bin/ccache /usr/libexec/ccache-wrappers/s390x-linux-gnu-gcc

RUN export DEBIAN_FRONTEND=noninteractive && \
    dpkg --add-architecture s390x && \
    eatmydata apt-get update && \
    eatmydata apt-get dist-upgrade -y && \
    eatmydata apt-get install --no-install-recommends -y dpkg-dev && \
    eatmydata apt-get install --no-install-recommends -y \
            g++-s390x-linux-gnu \
            gcc-s390x-linux-gnu \
            libc6-dev:s390x \
            libev-dev:s390x \
            libfuse3-dev:s390x \
            libglib2.0-dev:s390x \
            libgnutls28-dev:s390x \
            libxml2-dev:s390x && \
    eatmydata apt-get autoremove -y && \
    eatmydata apt-get autoclean -y && \
    mkdir -p /usr/local/share/meson/cross && \
    echo "[binaries]\n\
c = '/usr/bin/s390x-linux-gnu-gcc'\n\
ar = '/usr/bin/s390x-linux-gnu-gcc-ar'\n\
strip = '/usr/bin/s390x-linux-gnu-strip'\n\
pkgconfig = '/usr/bin/s390x-linux-gnu-pkg-config'\n\
\n\
[host_machine]\n\
system = 'linux'\n\
cpu_family = 's390x'\n\
cpu = 's390x'\n\
endian = 'big'" > /usr/local/share/meson/cross/s390x-linux-gnu

ENV LANG "en_US.UTF-8"
ENV MAKE "/usr/bin/make"
ENV CCACHE_WRAPPERSDIR "/usr/libexec/ccache-wrappers"

ENV ABI "s390x-linux-gnu"
ENV CONFIGURE_OPTS "--host=s390x-linux-gnu"
