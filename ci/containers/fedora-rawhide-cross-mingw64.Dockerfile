# THIS FILE WAS AUTO-GENERATED
#
#  $ lcitool dockerfile --cross mingw64 fedora-rawhide libnbd
#
# https://gitlab.com/libvirt/libvirt-ci/-/commit/5fd2cdeb8958df86a81b639fd096f626d0f5dbd0

FROM registry.fedoraproject.org/fedora:rawhide

RUN dnf update -y --nogpgcheck fedora-gpg-keys && \
    dnf install -y nosync && \
    echo -e '#!/bin/sh\n\
if test -d /usr/lib64\n\
then\n\
    export LD_PRELOAD=/usr/lib64/nosync/nosync.so\n\
else\n\
    export LD_PRELOAD=/usr/lib/nosync/nosync.so\n\
fi\n\
exec "$@"' > /usr/bin/nosync && \
    chmod +x /usr/bin/nosync && \
    nosync dnf update -y && \
    nosync dnf install -y \
        autoconf \
        automake \
        bash-completion \
        ca-certificates \
        ccache \
        coreutils \
        diffutils \
        git \
        glibc-langpack-en \
        gnutls \
        golang \
        iproute \
        jq \
        libtool \
        make \
        nbd \
        nbdkit \
        ocaml \
        ocaml-findlib \
        perl-base \
        python3-devel \
        python3-flake8 \
        qemu-img \
        qemu-kvm \
        sed && \
    nosync dnf autoremove -y && \
    nosync dnf clean all -y && \
    rpm -qa | sort > /packages.txt && \
    mkdir -p /usr/libexec/ccache-wrappers && \
    ln -s /usr/bin/ccache /usr/libexec/ccache-wrappers/x86_64-w64-mingw32-c++ && \
    ln -s /usr/bin/ccache /usr/libexec/ccache-wrappers/x86_64-w64-mingw32-cc && \
    ln -s /usr/bin/ccache /usr/libexec/ccache-wrappers/x86_64-w64-mingw32-g++ && \
    ln -s /usr/bin/ccache /usr/libexec/ccache-wrappers/x86_64-w64-mingw32-gcc

RUN nosync dnf install -y \
        mingw64-gcc \
        mingw64-gcc-c++ \
        mingw64-glib2 \
        mingw64-gnutls \
        mingw64-headers \
        mingw64-libxml2 \
        mingw64-pkg-config && \
    nosync dnf clean all -y

ENV LANG "en_US.UTF-8"
ENV MAKE "/usr/bin/make"
ENV CCACHE_WRAPPERSDIR "/usr/libexec/ccache-wrappers"

ENV ABI "x86_64-w64-mingw32"
ENV CONFIGURE_OPTS "--host=x86_64-w64-mingw32"
