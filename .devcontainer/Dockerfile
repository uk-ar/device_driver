# See here for image contents: https://github.com/microsoft/vscode-dev-containers/tree/v0.195.0/containers/cpp/.devcontainer/base.Dockerfile
# [Choice] Debian / Ubuntu version (use Debian 11/9, Ubuntu 18.04/21.04 on local arm64/Apple Silicon): debian-11, debian-10, debian-9, ubuntu-21.04, ubuntu-20.04, ubuntu-18.04
# ARG VARIANT=debian-11
ARG VARIANT=ubuntu-21.04
FROM mcr.microsoft.com/vscode/devcontainers/cpp:0-${VARIANT}

# [Optional] Uncomment this section to install additional packages.
RUN  apt-get update && export DEBIAN_FRONTEND=noninteractive \
        && apt-get -y install --no-install-recommends \
        build-essential make flex bison libncurses-dev git ccache unzip \
        cmake libpixman-1-dev libssl-dev sudo dwarves \
        linux-image-generic
RUN sed -i -e 's/^# deb-src /deb-src /g' /etc/apt/sources.list \
        && apt-get update -y \
        && apt build-dep -y linux linux-image-generic \
        && apt source linux
WORKDIR /workspaces/device_driver/
RUN sudo apt source linux
RUN apt-get install -y gdb qemu vim make
COPY .config linux-5.11.0/
RUN cd linux-5.11.0/ \
        && pwd \
        && make -j4
# gomi
#RUN apt-get -y install --no-install-recommends \
#         linux-source #\
# RUN tar xvf /usr/src/linux-source-*tar.bz2
