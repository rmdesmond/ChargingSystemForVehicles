#!/usr/bin/env bash
# Prove the build works from a clean clone: no leftover build directories, no
# uncommitted files. Builds and tests the native preset and the cross preset
# for the "other" architecture, then smoke-runs the foreign daemon under QEMU.
#
# Usage: scripts/verify.sh set -euo pipefail.  (run from anywhere inside the repo)
set -euo pipefail

repo_root=$(git -C "$(dirname "$0")" rev-parse --show-toplevel)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

# Pick the cross preset for the architecture this machine is NOT.
case "$(uname -m)" in
    aarch64) cross=cross-x86_64; qemu=qemu-x86_64-static; triple=x86_64-linux-gnu ;;
    x86_64) cross=cross-aarch64; qemu=qemu-aarch64-static; triple=aarch64-linux-gnu ;;
    *) echo "unsupported host $(uname -m)" >&2; exit 1 ;;
esac

echo "== clean clone of HEAD into $work"
git clone --quiet "$repo_root" "$work/src"
cd "$work/src"

start=$(date +%s)
for preset in native "$cross"; do
    echo "== $preset: configure, build, test"
    cmake --preset "$preset" > /dev/null
    cmake --build --preset "$preset"
    ctest --preset "$preset"
done

echo "== smoke-run the $cross daemon under QEMU"
"$qemu" -L "/usr/$triple" "build-$cross/app/charger-daemon"

echo "== OK: native and $cross built and tested from a clean clone in $(( $(date +%s) - start )) s"