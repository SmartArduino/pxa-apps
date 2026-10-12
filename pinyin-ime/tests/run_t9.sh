#!/usr/bin/env bash
set -euo pipefail
app_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
test_dir="$(mktemp -d "${TMPDIR:-/tmp}/pxa-ime-t9.XXXXXX")"
trap 'rm -rf "$test_dir"' EXIT
"${CC:-cc}" -std=c11 -Wall -Wextra -Werror -I"$app_dir" \
  "$app_dir/tests/t9_test.c" "$app_dir/core/ime_t9.c" \
  "$app_dir/core/ime_trie.c" -o "$test_dir/t9-test"
"$test_dir/t9-test"
