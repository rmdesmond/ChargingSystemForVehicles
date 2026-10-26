# Build image for the charger project: the one environment that CI builds in,
# and that any developer can reproduce with one command.
#
# docker build -t charger-build docker/
#
# Two kinds of pin, for two kinds of drift:
# 1. The base image is named by digest, not only by tag. A tag such as 24.04
# is moved to a new image every few weeks; a digest never changes.
# 2. Every package is installed at an exact version. If Ubuntu publishes a
# newer one, this build FAILS instead of silently changing the toolchain.
# That failure is the signal to review and bump the pins on purpose.
#
# Get the digest:   docker pull ubuntu:24.04 (prints "Digest: sha256:...")
# Get a version:    docker run --rm ubuntu:24.04 sh -c \
#                       'apt-get update -qq && apt-cache policy cmake'
FROM ubuntu:24.04@sha256:534baea6a22c03a63003dbc8dbe78fe34bc0d7e595d9a9dc9834884ff530eb55

# Stop apt from asking questions during the build only (ARG, not ENV, so it
# does not leak into containers started from the image).
ARG DEBIAN_FRONTEND=noninteractive

# One RUN, so the package index is fetched, used and deleted in a single layer.
# Both cross compilers are listed: on each architecture one of them is simply
# the native compiler under its full name, so the same list works on the
# aarch64 VM and on x86-64 CI runners.
# python3 itself arrives as a dependency of clang-tidy; python3-venv is what
# 'python3 -m venv' needs, so the SIL suite's pinned pytest can be installed.
RUN apt-get update \
  && apt-get install -y \
        build-essential=12.10ubuntu1 \
        ca-certificates=20260816ubuntu0.24.04.1 \
        ccache=4.9.1-1 \
        clang-format=1:18.0-59~exp2 \
        clang-tidy=1:18.0-59~exp2 \
        cmake=3.28.3-1build7 \
        g++-aarch64-linux-gnu=4:13.2.0-7ubuntu1 \
        g++-x86-64-linux-gnu=4:13.2.0-7ubuntu1 \
        git=1:2.43.0-1ubuntu7.4 \
        lcov=2.0-4ubuntu2 \
        libjson-xs-perl=4.040-0ubuntu0.24.04.1 \
        ninja-build=1.11.1-2 \
        python3-venv=3.12.3-0ubuntu2.1 \
        qemu-user-static=1:8.2.2+ds-0ubuntu1.18 \
&& rm -rf /var/lib/apt/lists/*

# The compiler cache lives outside the image, on a volume, so it survives from
# one container run to the next. World-writable with the sticky bit (like
# /tmp) because the container is started with the caller's user id.
RUN mkdir /ccache && chmod 1777 /ccache
ENV CCACHE_DIR=/ccache
VOLUME /ccache

# Do not build as root. 'ubuntu' (uid 1000) ships with the base image.
USER ubuntu
WORKDIR /work