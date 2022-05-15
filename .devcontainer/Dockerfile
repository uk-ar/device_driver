# See here for image contents: https://github.com/microsoft/vscode-dev-containers/tree/v0.195.0/containers/cpp/.devcontainer/base.Dockerfile
# [Choice] Debian / Ubuntu version (use Debian 11/9, Ubuntu 18.04/21.04 on local arm64/Apple Silicon): debian-11, debian-10, debian-9, ubuntu-21.04, ubuntu-20.04, ubuntu-18.04
# ARG VARIANT=debian-11
ARG VARIANT=debian-11
FROM mcr.microsoft.com/vscode/devcontainers/cpp:0-${VARIANT}

# [Optional] Uncomment this section to install additional packages.
RUN  apt-get update && export DEBIAN_FRONTEND=noninteractive \
        && apt-get -y install --no-install-recommends \
        bc \
        build-essential \
        bzr \
        ca-certificates \
        cmake \
        cpio \
        cvs \
        file \
#        g++-multilib \
        git \
#        libc6:i386 \
        libncurses5-dev \
        locales \
        mercurial \
        openssh-server \
        python3 \
        python3-flake8 \
        python3-nose2 \
        python3-pexpect \
        python3-pytest \
        qemu-system-arm \
        qemu-system-x86 \
        rsync \
        shellcheck \
        subversion \
        unzip \
        wget \
        && \
    apt-get -y autoremove && \
    apt-get -y clean

# To be able to generate a toolchain with locales, enable one UTF-8 locale
RUN sed -i 's/# \(en_US.UTF-8\)/\1/' /etc/locale.gen && \
    /usr/sbin/locale-gen

ENV LC_ALL en_US.UTF-8

RUN wget https://buildroot.org/downloads/buildroot-2022.02.1.tar.gz \
        && tar zxvf buildroot-*.tar.gz \
        && mv buildroot-2022.02.1 /buildroot \
        && rm buildroot-*.tar.gz
VOLUME /buildroot/dl
VOLUME /buildroot/output
VOLUME /root/.buildroot-ccache
# buildroot output
# RUN mkdir output \
# #        && mkdir output/host \
# #        && mkdir output/target \
#         && chmod a+w output \
# #        && chmod a+w output/host \
# #        && chmod a+w output/target \
#         && chmod a+w buildroot-2022.02.1
