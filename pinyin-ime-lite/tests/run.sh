#!/usr/bin/env bash
set -euo pipefail
app_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
system_dir="${PXA_SYSTEM_DIR:-$(cd "$app_dir/../../.." && pwd)/deps/pxa-system}"
work_dir="$(mktemp -d "${TMPDIR:-/tmp}/pxa-ime-lite-test.XXXXXX")"
trap 'rm -rf "$work_dir"' EXIT
python3 "$app_dir/tools/generate_table.py" --check
"${CXX:-clang++}" -std=c++2c -O2 -Wall -Wextra -Werror \
  -fsanitize=address,undefined -fno-omit-frame-pointer \
  "$app_dir/tests/engine_test.cpp" -o "$work_dir/engine-test"
"$work_dir/engine-test"
"${CC:-cc}" -std=c11 -O2 -Wall -Wextra -Werror -I"$app_dir/../pinyin-ime" \
  -fsanitize=address,undefined -c "$app_dir/../pinyin-ime/core/ime_t9.c" -o "$work_dir/t9.o"
"${CC:-cc}" -std=c11 -O2 -Wall -Wextra -Werror -I"$app_dir/../pinyin-ime" \
  -fsanitize=address,undefined -c "$app_dir/../pinyin-ime/core/ime_trie.c" -o "$work_dir/trie.o"
"${CXX:-clang++}" -std=c++2c -O2 -Wall -Wextra -Werror -Wno-attributes \
  -fsanitize=address,undefined -fno-omit-frame-pointer -fno-exceptions -fno-rtti \
  -I"$system_dir/sdk/guest-cpp/include" -I"$system_dir/sdk/testing" \
  "$app_dir/tests/session_test.cpp" "$system_dir/sdk/guest-cpp/src/runtime.cpp" \
  "$work_dir/t9.o" "$work_dir/trie.o" -o "$work_dir/session-test"
"$work_dir/session-test"
bash "$app_dir/../pinyin-ime/tests/run_t9.sh"
echo 'PXA lite: complete character table, segmentation, aliases, bounds and 10000 stress inputs passed'
