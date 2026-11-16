#!/usr/bin/env bash
# The key string inside scripts/install-release.sh must be the key in
# keys/release.pub (line 2 of a minisign public key file).
set -euo pipefail
cd "$(git -C "$(dirname "$0")" rev-parse --show-toplevel)"
committed=$(sed -n '2p' keys/release.pub)
embedded=$(sed -n 's/^RELEASE_PUBKEY="\(.*\)"$/\1/p' scripts/install-release.sh)
if [ "$committed" != "$embedded" ]; then
    echo "keys/release.pub:            $committed"
    echo "scripts/install-release.sh:  $embedded"
    echo "FAIL: the installer does not embed the committed public key"
    exit 1
fi
echo "ok  installer embeds $committed"
