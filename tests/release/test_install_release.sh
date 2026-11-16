#!/usr/bin/env bash
# REQ-020: the installer accepts a signed release and refuses a tampered,
# unsigned or wrongly signed one without touching the installed release.
#
# tests/release/test_install_release.sh <charger-daemon binary>
#
# Runs the real scripts (package.sh, sign-release.sh, install-release.sh) with
# a throwaway key pair, against a temporary prefix. Needs minisign, sha256sum,
# tar and a git checkout (package.sh asks git for the version).
set -euo pipefail
daemon=$(realpath "$1")
cd "$(git -C "$(dirname "$0")" rev-parse --show-toplevel)"

t=$(mktemp -d)
trap 'rm -rf "$t"' EXIT
export MINISIGN_PASSWORD=test-only
export CHARGER_RELEASE_PUBKEY_FILE="$t/test.pub"
printf '%s\n%s\n' "$MINISIGN_PASSWORD" "$MINISIGN_PASSWORD" \
    | minisign -G -f -p "$t/test.pub" -s "$t/test.key" > /dev/null
MINISIGN_SECRET_KEY=$(cat "$t/test.key")
export MINISIGN_SECRET_KEY

install() {
    scripts/install-release.sh --from "$1" --prefix "$t/opt" --public-key "$t/test.pub" --no-service
}

# A refusal must exit non-zero, say REFUSED, and leave current where it was.
expect_refusal() {
    local what="$1" dir="$2" before
    before=$(readlink "$t/opt/current")
    if install "$dir" > "$t/out" 2>&1; then
        echo "FAIL: $what was installed"; cat "$t/out"; exit 1
    fi
    grep -q '^REFUSED:' "$t/out" || { echo "FAIL: $what: no REFUSED line"; cat "$t/out"; exit 1; }
    [ "$(readlink "$t/opt/current")" = "$before" ] \
        || { echo "FAIL: $what moved the current symlink"; exit 1; }
    echo "ok  refused: $what ($(sed -n 's/^REFUSED: //p' "$t/out"))"
}

echo "== package and sign a release with the test key"
scripts/package.sh --binary "$daemon" --out "$t/good" > /dev/null
scripts/sign-release.sh "$t/good" > /dev/null

echo "== a good release installs"
install "$t/good" > "$t/out" 2>&1 || { cat "$t/out"; exit 1; }
expected=$("$daemon" --version)
installed=$("$t/opt/current/bin/charger-daemon" --version)
[ "$installed" = "$expected" ] || { echo "FAIL: installed '$installed', built '$expected'"; exit 1; }
grep -q '^version=' "$t/opt/current/VERIFIED" || { echo "FAIL: no VERIFIED marker"; exit 1; }
echo "ok  installed $installed, marker written"

echo "== tampered tarball: one byte changed"
cp -r "$t/good" "$t/tampered"
tarball=$(ls "$t"/tampered/charger-daemon-*.tar.gz)
printf '\x00' | dd of="$tarball" bs=1 seek=100 conv=notrunc 2> /dev/null
expect_refusal "tampered tarball" "$t/tampered"

echo "== unsigned: the .minisig files removed"
cp -r "$t/good" "$t/unsigned"
rm "$t"/unsigned/*.minisig
expect_refusal "unsigned release" "$t/unsigned"

echo "== signed with a different key"
cp -r "$t/good" "$t/otherkey"
rm "$t"/otherkey/*.minisig
# -W: no password on this key; each scrypt derivation costs about 3 s and
# the password path is already exercised by the key above.
minisign -G -W -f -p "$t/other.pub" -s "$t/other.key" > /dev/null
MINISIGN_SECRET_KEY=$(cat "$t/other.key") MINISIGN_PASSWORD=unused \
    CHARGER_RELEASE_PUBKEY_FILE="$t/other.pub" scripts/sign-release.sh "$t/otherkey" > /dev/null
expect_refusal "release signed with another key" "$t/otherkey"

echo "== edited SHA256SUMS with its signature left in place"
cp -r "$t/good" "$t/sums"
sed -i 's/^./0/' "$t/sums/SHA256SUMS"
expect_refusal "edited checksum manifest" "$t/sums"

echo "PASS: REQ-020"
