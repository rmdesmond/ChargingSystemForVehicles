#!/usr/bin/env bash
# Build the aarch64 daemon and package it as a versioned, reproducible tarball.
#
# scripts/package.sh                       build, then package into dist/
# scripts/package.sh --binary FILE         package an already built daemon
# scripts/package.sh --out DIR             write somewhere other than dist/
#
# Output, for version v0.1.0:
#   dist/charger-daemon-v0.1.0-aarch64.tar.gz    the release
#   dist/SHA256SUMS                               checksum of the tarball
# Inside the tarball, one directory charger-daemon-v0.1.0/ with bin/, systemd/,
# scripts/install-release.sh, MANIFEST, VERSION and its own SHA256SUMS. The
# release job signs both files; install-release.sh verifies them on the device.
#
# On the aarch64 VM the native build IS the Pi build; on an x86-64 machine
# (CI) the cross-aarch64 preset is used. The tarball is reproducible: build the
# same commit twice and the bytes are identical, so a checksum means something.
set -euo pipefail
cd "$(git -C "$(dirname "$0")" rev-parse --show-toplevel)"

binary=""
out=dist
while [ $# -gt 0 ]; do
    case "$1" in
        --binary) binary="$2"; shift 2 ;;
        --out) out="$2"; shift 2 ;;
        *) sed -n '2,12p' "$0" | sed 's/^# \{0,1\}//' >&2; exit 2 ;;
    esac
done

# The same question cmake/GenerateVersion.cmake asks at build time, so the
# file name and the string inside the binary cannot disagree.
version=$(git describe --tags --always --dirty --match 'v*')
commit=$(git rev-parse HEAD)
# Timestamps inside the archive come from the commit, not from the clock.
epoch=$(git log -1 --format=%ct)
arch=aarch64
name="charger-daemon-$version"

if [ -z "$binary" ]; then
    case "$(uname -m)" in
        aarch64) preset=native ;;
        *) preset=cross-aarch64 ;;
    esac
    echo "== build ($preset)"
    cmake --preset "$preset" > /dev/null
    cmake --build --preset "$preset" --target charger-daemon
    binary="build-$preset/app/charger-daemon"
fi

echo "== stage $name"
stage=$(mktemp -d)
trap 'rm -rf "$stage"' EXIT
root="$stage/$name"
install -D --mode=0755 "$binary" "$root/bin/charger-daemon"
install -D --mode=0644 systemd/charger-daemon.service "$root/systemd/charger-daemon.service"
# The installer travels with the release for convenience; the copy that is
# trusted is the one already on the device (it embeds the public key).
######## RANDALL ### install -D --mode=0755 scripts/install-release.sh "$root/scripts/install-release.sh"
echo "$version" > "$root/VERSION"
# Key=value, one per line: what a device or a person needs to know before
# installing. 'hardware' is the compatibility string the OTA bundle reuses.
cat > "$root/MANIFEST" <<EOF
name=charger-daemon
version=$version
commit=$commit
arch=$arch
hardware=rpi5 rpi3b+
built=$(date --utc --date="@$epoch" +%Y-%m-%dT%H:%M:%SZ)
EOF
# Checksums of the contents, inside the archive, so a partial extraction or a
# corrupted file is caught after unpacking too.
(cd "$root" && find . -type f | sort | sed 's|^\./||' | xargs sha256sum > "$stage/sums" \
    && mv "$stage/sums" SHA256SUMS)

echo "== pack"
mkdir -p "$out"
tarball="$out/$name-$arch.tar.gz"
# Sorted names, fixed times and ownership, no gzip timestamp: reproducible.
tar --create --directory "$stage" \
    --sort=name --mtime="@$epoch" --owner=0 --group=0 --numeric-owner \
    "$name" | gzip --no-name > "$tarball"
(cd "$out" && sha256sum "$(basename "$tarball")" > SHA256SUMS)

echo "== done"
ls -l "$tarball" "$out/SHA256SUMS"
cat "$root/MANIFEST"
