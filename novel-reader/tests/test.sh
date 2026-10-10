#!/usr/bin/env bash
set -euo pipefail
app_dir="$(cd "$(dirname "$0")/.." && pwd)"
workspace="$(cd "$app_dir/../../.." && pwd)"
out="$workspace/local/reader-tests"
mkdir -p "$out"
compiler="${CXX:-clang++}"
flags=(-std=c++2c -Wno-attributes -O3 -Wall -Wextra -Werror -Wno-misleading-indentation -I"$workspace/deps/pxa-system/sdk/guest-cpp/include")
if [[ "${SANITIZE:-0}" == 1 ]]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer -g); fi
"$compiler" "${flags[@]}" "$app_dir/tests/core_test.cpp" -o "$out/core_test"
"$out/core_test"
