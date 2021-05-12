# THIS FILE WAS AUTO-GENERATED
#
#  $ lcitool dockerfile --cross armv7l debian-sid libnbd
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
    ln -s /usr/bin/ccache /usr/libexec/ccache-wrappers/arm-linux-gnueabihf-c++ && \
    ln -s /usr/bin/ccache /usr/libexec/ccache-wrappers/arm-linux-gnueabihf-cc && \
    ln -s /usr/bin/ccache /usr/libexec/ccache-wrappers/arm-linux-gnueabihf-g++ && \
    ln -s /usr/bin/ccache /usr/libexec/ccache-wrappers/arm-linux-gnueabihf-gcc

RUN export DEBIAN_FRONTEND=noninteractive && \
    dpkg --add-architecture armhf && \
    eatmydata apt-get update && \
    eatmydata apt-get dist-upgrade -y && \
    eatmydata apt-get install --no-install-recommends -y dpkg-dev && \
    eatmydata apt-get install --no-install-recommends -y \
            g++-arm-linux-gnueabihf \
            gcc-arm-linux-gnueabihf \
            libc6-dev:armhf \
            libev-dev:armhf \
            libfuse3-dev:armhf \
            libglib2.0-dev:armhf \
            libgnutls28-dev:armhf \
            libxml2-dev:armhf && \
    eatmydata apt-get autoremove -y && \
    eatmydata apt-get autoclean -y && \
    mkdir -p /usr/local/share/meson/cross && \
    echo "[binaries]\n\
c = '/usr/bin/arm-linux-gnueabihf-gcc'\n\
ar = '/usr/bin/arm-linux-gnueabihf-gcc-ar'\n\
strip = '/usr/bin/arm-linux-gnueabihf-strip'\n\
pkgconfig = '/usr/bin/arm-linux-gnueabihf-pkg-config'\n\
\n\
[host_machine]\n\
system = 'linux'\n\
cpu_family = 'arm'\n\
cpu = 'armhf'\n\
endian = 'little'" > /usr/local/share/meson/cross/arm-linux-gnueabihf

ENV LANG "en_US.UTF-8"
ENV MAKE "/usr/bin/make"
ENV CCACHE_WRAPPERSDIR "/usr/libexec/ccache-wrappers"

ENV ABI "arm-linux-gnueabihf"
ENV CONFIGURE_OPTS "--host=arm-linux-gnueabihf"
