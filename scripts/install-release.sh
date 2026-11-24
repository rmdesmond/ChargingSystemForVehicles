#!/usr/bin/env bash
# Install a signed charger-daemon release on the device, or refuse it.
#
# sudo scripts/install-release.sh --tag v0.1.0   // download from GitHub
# sudo scripts/install-release.sh --from DIR     // from files already here
#
# Options: --prefix DIR          // install root (default /opt/charger)
#          --repo OWNER/NAME     // GitHub repository to download from
#          --public-key FILE     // trust this key instead of the embedded one
#          --no-service          // do not touch systemd (tests, or a manual check)
#
# Nothing is installed until the release has passed, in this order:
#   1. the signature on SHA256SUMS (minisign, the key embedded below)
#   2. the checksum of the tarball (sha256sum -c)
#   3. the signature on the tarball itself
#   4. the checksums inside the tarball, after unpacking into a staging
#      directory, and the MANIFEST version against the requested tag
# A failure at any step prints REFUSED and leaves the current release, its
# symlink and the running service exactly as they were (REQ-020).
#
# Layout: PREFIX/releases/<version>/{bin,systemd,MANIFEST,VERSION,SHA256SUMS}
#         PREFIX/releases/<version>/VERIFIED written here, read by the daemon
#         PREFIX/current -> releases/<version> flipped last, atomically
set -euo pipefail

# The release public key. A key that travels with the tarball could be
# replaced together with it; this one changes only through a reviewed pull
# request, and a test checks it matches keys/release.pub.
RELEASE_PUBKEY="RWSzub/TpSmtxAzahLCiDDrEI6975VAEMyaQuy2MFm1fWXEIBkqnKZbV"

prefix=/opt/charger
repo=rmdesmond/charging-system-for-vehicles
tag=""
from=""
pubkey_file=""
service=yes
while [ $# -gt 0 ]; do
    case "$1" in
        --tag) tag="$2"; shift 2 ;;
        --from) from="$2"; shift 2 ;;
        --prefix) prefix="$2"; shift 2 ;;
        --repo) repo="$2"; shift 2 ;;
        --public-key) pubkey_file="$2"; shift 2 ;;
        --no-service) service=no; shift ;;
        *) sed -n '2,10p' "$0" | sed 's/^# \{0,1\}//' >&2; exit 2 ;;
    esac
done
if [ -z "$tag" ] && [ -z "$from" ]; then
    echo "install-release.sh: give --tag VERSION or --from DIR" >&2
    exit 2
fi
if [ -n "$pubkey_file" ]; then
    RELEASE_PUBKEY=$(sed -n '2p' "$pubkey_file")
fi

work=$(mktemp -d)
incoming="$prefix/releases/.incoming.$$"
cleanup() { rm -rf "$work" "$incoming"; }
trap cleanup EXIT

refuse() {
    echo "REFUSED: $*" >&2
    echo "nothing was changed; $prefix/current is untouched" >&2
    exit 1
}

# --- fetch -------------------------------------------------------------------
if [ -n "$from" ]; then
    cp "$from"/charger-daemon-*.tar.gz* "$from"/SHA256SUMS* "$work/" 2> /dev/null \
        || refuse "$from does not hold a complete release (tarball, SHA256SUMS, two .minisig)"
    tag=$(basename "$work"/charger-daemon-*.tar.gz \
        | sed -E 's/^charger-daemon-(.*)-[^-]+\.tar\.gz$/\1/')
else
    base="https://github.com/$repo/releases/download/$tag"
    arch=$(uname -m)
    for file in "charger-daemon-$tag-$arch.tar.gz" "charger-daemon-$tag-$arch.tar.gz.minisig" \
                SHA256SUMS SHA256SUMS.minisig; do
        echo "== fetch $file"
        curl --fail --silent --show-error --location --output "$work/$file" "$base/$file" \
            || refuse "cannot download $file from $base"
    done
fi
tarball=$(ls "$work"/charger-daemon-*.tar.gz)
[ "$(echo "$tarball" | wc -l)" = 1 ] || refuse "more than one tarball"

# --- verify, before anything is unpacked ------------------------------------
echo "== verify"
[ -f "$work/SHA256SUMS.minisig" ] || refuse "SHA256SUMS is not signed"
minisign -V -q -P "$RELEASE_PUBKEY" -m "$work/SHA256SUMS" \
    || refuse "bad signature on SHA256SUMS"
(cd "$work" && sha256sum --check --quiet --strict SHA256SUMS) \
    || refuse "checksum mismatch: the tarball is not the one that was signed"
[ -f "$tarball.minisig" ] || refuse "the tarball is not signed"
minisign -V -q -P "$RELEASE_PUBKEY" -m "$tarball" \
    || refuse "bad signature on the tarball"
signer=$(minisign -V -Q -P "$RELEASE_PUBKEY" -m "$tarball")
echo "signed: $signer"

# --- unpack into a staging directory and check the contents -----------------
echo "== unpack"
mkdir -p "$prefix/releases"
mkdir "$incoming"
tar --extract --gzip --file "$tarball" --directory "$incoming" --strip-components=1 \
    || refuse "the tarball does not unpack"
(cd "$incoming" && sha256sum --check --quiet --strict SHA256SUMS) \
    || refuse "checksum mismatch inside the tarball"
version=$(sed -n 's/^version=//p' "$incoming/MANIFEST")
[ "$version" = "$tag" ] || refuse "MANIFEST says version $version, expected $tag"
[ -x "$incoming/bin/charger-daemon" ] || refuse "no daemon binary in the release"

# --- install: the only steps that change the device ---------------------------
echo "== install $version"
{
    echo "version=$version"
    echo "verified=$(date --utc +%Y-%m-%dT%H:%M:%SZ)"
    echo "signer=$signer"
} > "$incoming/VERIFIED"
target="$prefix/releases/$version"
rm -rf "$target"
mv "$incoming" "$target"
# ln then rename: the symlink is replaced in one step, never missing.
ln -s "releases/$version" "$prefix/current.new"
mv -T "$prefix/current.new" "$prefix/current"

if [ "$service" = yes ] && command -v systemctl > /dev/null; then
    echo "== service"
    id charger > /dev/null 2>&1 \
        || useradd --system --no-create-home --shell /usr/sbin/nologin charger
    install -D --mode=0644 "$target/systemd/charger-daemon.service" \
        /etc/systemd/system/charger-daemon.service
    systemctl daemon-reload
    systemctl enable --quiet charger-daemon
    systemctl restart charger-daemon
    systemctl --no-pager --lines=0 status charger-daemon
fi
echo "== installed"
"$prefix/current/bin/charger-daemon" --version
