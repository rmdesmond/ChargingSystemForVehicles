#!/usr/bin/env bash
# Rewrite every tracked C++ file in place to match .clang-format.
set -euo pipefail
cd "$(git -C "$(dirname "$0")" rev-parse --show-toplevel)"
git ls-files '*.h' '*.cpp' | xargs clang-format -i