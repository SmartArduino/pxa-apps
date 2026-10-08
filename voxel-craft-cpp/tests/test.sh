#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../../.." && pwd)"
app="$root/local/pxa-apps/voxel-craft-cpp"
sdk="$root/deps/pxa-system/sdk/guest-cpp"
build="$(mktemp -d /tmp/voxel-cpp-test.XXXXXX)"
trap 'rm -rf "$build"' EXIT
flags=(-std=c++2c -O2 -fno-exceptions -fno-rtti -I"$sdk/include" -Wno-attributes)
"${CXX:-clang++}" "${flags[@]}" "$app/tests/input_test.cpp" "$app/voxel_world.cpp" "$app/voxel_render.cpp" "$app/voxel_mesher.cpp" "$sdk"/src/*.cpp -o "$build/input"
"$build/input"
"${CXX:-clang++}" "${flags[@]}" "$app/tests/world_test.cpp" "$app/voxel_world.cpp" -o "$build/world"
"$build/world"
printf 'Voxel C++ input tests OK\n'
