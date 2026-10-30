#!/bin/sh
# SPDX-License-Identifier: CC0-1.0
set -eu
app=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
out=${1:?usage: tests/run-native.sh /absolute/path/to/local/artifacts/topic/timestamp}
mkdir -p "$out"
flags='-DPXA_POCKET_UTILITY=1 -std=c++17 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie'
${CXX:-c++} $flags "$app/main.cpp" "$app/model.cpp" "$app/tests/runtime.cpp" "$app/tests/native.cpp" -o "$out/native"
"$out/native"
