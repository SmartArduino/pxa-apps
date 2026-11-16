#!/usr/bin/env bash
set -euo pipefail
app=$(cd "$(dirname "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
"${CXX:-clang++}" -std=c++2c -O1 -g -Wall -Wextra -Werror \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    "$app/tests/gmanhua_test.cpp" "$app/browser_script_common.cpp" -o "$work/test"
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 "$work/test" "$@"
