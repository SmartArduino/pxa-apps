#!/bin/sh
# SPDX-License-Identifier: CC0-1.0
set -eu
app=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
out=${1:?provide local/artifacts/calculator/timestamp/native}
mkdir -p "$out"
${CXX:-c++} -DPXA_POCKET_UTILITY=1 -std=c++17 -O1 -g -Wall -Wextra -Werror -Wno-format-truncation -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie "$app/main.cpp" "$app/model.cpp" "$app/engine.cpp" "$app/tests/runtime.cpp" "$app/tests/native.cpp" -o "$out/native"
"$out/native"
