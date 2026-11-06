#!/usr/bin/env bash
set -euo pipefail
source_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
binary="$(mktemp)"
trap 'rm -f "$binary"' EXIT
"${CXX:-clang++}" -std=c++23 -Wall -Wextra -Werror -fsanitize=address,undefined \
  -fno-omit-frame-pointer "$source_dir/media_test.cpp" -o "$binary"
"$binary"
