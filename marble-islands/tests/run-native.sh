#!/bin/sh
# SPDX-License-Identifier: CC0-1.0
set -eu
source_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
output=${1:?usage: sh tests/run-native.sh /absolute/path/to/local/artifacts/topic/timestamp}
mkdir -p "$output"
flags='-std=c++17 -O2 -Wall -Wextra -Werror'
if [ "${SANITIZE:-0}" = 1 ]; then
    flags='-std=c++17 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer'
fi
# Word splitting is intentional for this fixed set of compiler flags.
${CXX:-c++} $flags "$source_dir/core.cpp" "$source_dir/renderer.cpp" "$source_dir/tests/native.cpp" -o "$output/native"
"$output/native" "$output"
${CXX:-c++} $flags "$source_dir/core.cpp" "$source_dir/renderer.cpp" "$source_dir/main.cpp" "$source_dir/tests/app.cpp" -o "$output/app"
"$output/app" "$output"
