#!/usr/bin/env bash
#
# Sign a packaged release with minisign, then verify the signatures with the
# committed public key before anything is published.
#
# scripts/sign-release.sh [DIR]            default DIR: dist
#
# Needs, in the environment (GitHub secrets in CI, your own shell locally):
#
# MINISIGN_SECRET_KEY the whole secret key file, as text
# MINISIGN_PASSWORD its password
#
# Writes DIR/<tarball>.minisig and DIR/SHA256SUMS.minisig. The trusted comment
# names the release, so a signature cannot be moved to another version's file.
#
# The secret key exists on disk only inside a private temporary directory for
# the seconds this script runs, and never in the repository.

set -euo pipefail
dir="${1:-dist}"
cd "$(git -C "$(dirname "$0")" rev-parse --show-toplevel)"

: "${MINISIGN_SECRET_KEY:?set MINISIGN_SECRET_KEY to the secret key file, as text}"
: "${MINISIGN_PASSWORD:?set MINISIGN_PASSWORD to the password of that key}"
pubkey="${CHARGER_RELEASE_PUBKEY_FILE:-keys/release.pub}"

tarball=$(ls "$dir"/charger-daemon-*.tar.gz)
version=$(basename "$tarball" | sed -E 's/^charger-daemon-(.*)-[^-]+\.tar\.gz$/\1/')
commit=$(git rev-parse --short HEAD)

keydir=$(mktemp -d)
trap 'rm -rf "$keydir"' EXIT
(umask 077 && printf '%s\n' "$MINISIGN_SECRET_KEY" > "$keydir/release.key")

echo "== sign $version"
for file in "$tarball" "$dir/SHA256SUMS"; do
    # minisign reads the password from stdin when stdin is not a terminal.
    printf '%s\n' "$MINISIGN_PASSWORD" | minisign -S -s "$keydir/release.key" \
        -t "charger-daemon $version ($commit)" -m "$file" > /dev/null
done

echo "== verify with $pubkey"
for file in "$tarball" "$dir/SHA256SUMS"; do
    minisign -V -q -p "$pubkey" -m "$file"
    echo "ok $(basename "$file")"
done
