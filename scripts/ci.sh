#!/usr/bin/env bash
# The CI pipeline's steps, in one place. GitHub Actions runs exactly these
# commands, inside the build image; so can you:
#
# scripts/ci.sh build configure and build native + cross; cross tests under QEMU
# scripts/ci.sh lint clang-format check, clang-tidy, pyflakes on the harness
# scripts/ci.sh test unit tests with coverage (report in build-coverage/coverage/html)
# scripts/ci.sh sil SIL suite: the native daemon driven by pytest
# scripts/ci.sh all the four above, in order
set -euo pipefail
cd "$(git -C "$(dirname "$0")" rev-parse --show-toplevel)"

# The cross preset is the one for the architecture this machine is NOT.
case "$(uname -m)" in
    aarch64) cross=cross-x86_64 ;;
    x86_64) cross=cross-aarch64 ;;
    *) echo "unsupported host $(uname -m)" >&2; exit 1 ;;
esac

# The Python tools (pytest, pyflakes) come from a venv pinned by
# requirements-test.txt, created on first use. CHARGER_VENV points elsewhere
# if you keep yours outside the tree. Prints the venv's bin directory.
python_bin() {
    local venv="${CHARGER_VENV:-.venv}"
    if [ ! -x "$venv/bin/pytest" ] || [ ! -x "$venv/bin/pyflakes" ]; then
        python3 -m venv "$venv"
        "$venv/bin/pip" install --quiet --requirement requirements-test.txt
    fi
    echo "$venv/bin"
}

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
    # The harness is product code too: it is what the SIL gate trusts.
    "$(python_bin)/pyflakes" tools tests/sil
    echo "pyflakes: OK"
}

ci_test() {
    cmake --preset coverage
    cmake --build --preset coverage --target coverage
}

# The junit file is the record CI keeps: it carries each test's captured
# output, including the daemon log the fixture prints on a failure.
ci_sil() {
    cmake --preset native
    cmake --build --preset native --target charger-daemon
    mkdir -p build-sil
    "$(python_bin)/pytest" --quiet --junitxml=build-sil/junit.xml tests/sil
}

case "${1:-}" in
    build) ci_build ;;
    lint) ci_lint ;;
    test) ci_test ;;
    sil) ci_sil ;;
    all) ci_build; ci_lint; ci_test; ci_sil ;;
    *) sed -n '2,10p' "$0" | sed 's/^# \{0,1\}//'; exit 2 ;;
esac