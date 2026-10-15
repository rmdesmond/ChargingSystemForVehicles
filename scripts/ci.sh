#!/usr/bin/env bash
# The CI pipeline's steps, in one place. GitHub Actions runs exactly these
# commands, inside the build image; so can you:
#
# scripts/ci.sh build configure and build native + cross; cross tests under QEMU
# scripts/ci.sh lint clang-format check and clang-tidy
# scripts/ci.sh test unit tests with coverage (report in build-coverage/coverage/html)
# scripts/ci.sh all the three above, in order
set -euo pipefail
cd "$(git -C "$(dirname "$0")" rev-parse --show-toplevel)"

# The cross preset is the one for the architecture this machine is NOT.
case "$(uname -m)" in
    aarch64) cross=cross-x86_64 ;;
    x86_64) cross=cross-aarch64 ;;
    *) echo "unsupported host $(uname -m)" >&2; exit 1 ;;
esac

ci_build() {
    for preset in native "$cross"; do
        cmake --preset "$preset"
        cmake --build --preset "$preset"
    done
    ctest --preset "$cross"
}

ci_lint() {
    scripts/check-format.sh
    cmake --preset native
    cmake --build --preset native --target tidy
}

ci_test() {
    cmake --preset coverage
    cmake --build --preset coverage --target coverage
}


case "${1:-}" in
    build) ci_build ;;
    lint) ci_lint ;;
    test) ci_test ;;
    all) ci_build; ci_lint; ci_test ;;
    *) sed -n '2,9p' "$0" | sed 's/^# \{0,1\}//'; exit 2 ;;
esac