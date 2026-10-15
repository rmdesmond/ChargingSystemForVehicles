#!/usr/bin/env bash
# Fail if any tracked C++ file differs from what clang-format would write.
# Changes nothing. Used by the pre-commit hook and by CI.
set -euo pipefail
cd "$(git -C "$(dirname "$0")" rev-parse --show-toplevel)"
if git ls-files '*.h' '*.cpp' | xargs clang-format --dry-run --Werror; then
    echo "format: OK"
else
    echo "format: FAILED. Run scripts/format.sh and commit again." >&2
    exit 1
fi