#!/usr/bin/env bash
# Run a command inside the pinned build image, with this repository mounted.
#
# scripts/build-in-docker.sh                       same as: scripts/ci.sh build
# scripts/build-in-docker.sh scripts/ci.sh test
# scripts/build-in-docker.sh bash                  look around (add -it yourself)
set -euo pipefail

repo=$(git -C "$(dirname "$0")" rev-parse --show-toplevel)
image=charger-build
ccache_dir="${CHARGER_CCACHE_DIR:-$HOME/.cache/charger-ccache}"
mkdir -p "$ccache_dir"

# Cheap when nothing changed: Docker reuses its cached layers.
docker build --quiet --tag "$image" "$repo/docker" > /dev/null

if [ "$#" -eq 0 ]; then
    set -- scripts/ci.sh build
fi

# --user: files written into the mounted repo belong to you, not to root.
# HOME=/tmp: that user id has no home directory inside the image.
# The repo is mounted at the SAME path as on the host. CMake records absolute
# paths in its cache, so a build directory stays valid inside and outside.
docker run --rm \
    --user "$(id -u):$(id -g)" \
    --env HOME=/tmp \
    --volume "$repo:$repo" \
    --volume "$ccache_dir:/ccache" \
    --workdir "$repo" \
    "$image" "$@"