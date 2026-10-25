#!/bin/sh
# SPDX-License-Identifier: CC0-1.0
# Keep the historical runner name; test event cues, never a background score.
# All generated binaries go exclusively into the caller's artifact directory.
set -eu
source_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
output=${1:?usage: sh tests/run-music.sh /absolute/path/to/local/artifacts/topic/timestamp}
case "$output" in
    /*) ;;
    *) printf '%s\n' 'Output must be an absolute caller artifact directory.' >&2; exit 2 ;;
esac
mkdir -p "$output"
"${CXX:-c++}" -std=c++17 -O1 -g -Wall -Wextra -Werror \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    "$source_dir/tests/music.cpp" -o "$output/music"
ASAN_OPTIONS=${ASAN_OPTIONS:-detect_leaks=1:halt_on_error=1}
UBSAN_OPTIONS=${UBSAN_OPTIONS:-halt_on_error=1:print_stacktrace=1}
export ASAN_OPTIONS UBSAN_OPTIONS
"$output/music" "$output"
