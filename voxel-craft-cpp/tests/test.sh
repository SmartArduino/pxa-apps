#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../../.." && pwd)"
app="$root/local/pxa-apps/voxel-craft-cpp"
sdk="$root/deps/pxa-system/sdk/guest-cpp"
build="$(mktemp -d /tmp/voxel-cpp-test.XXXXXX)"
trap 'rm -rf "$build"' EXIT
flags=(-std=c++2c -O2 -fno-exceptions -fno-rtti -I"$sdk/include" -Wno-attributes)
for setting in VOXEL_CHUNK_SIZE VOXEL_CHUNK_QUADS VOXEL_FIXED_ATTRIBUTES; do
    value=${!setting:-}
    if [[ -n "$value" ]]; then
        [[ "$value" =~ ^[1-9][0-9]*$ ]] || exit 2
        flags+=("-D$setting=$value")
    fi
done
if [[ "${VOXEL_PROFILE:-0}" == 1 ]]; then
    flags+=(-DVOXEL_PROFILE=1)
fi
if [[ "${SANITIZE:-0}" == 1 ]]; then
    flags+=(-g -fsanitize=address,undefined -fno-omit-frame-pointer)
    export UBSAN_OPTIONS=halt_on_error=1
fi
python3 "$app/tools/generate_lod_colors.py" --check
host="$root/deps/pxa-system/libpxa"
cflags=(-std=c11 -O2 -I"$host/include")
if [[ "${SANITIZE:-0}" == 1 ]]; then
    cflags+=(-g -fsanitize=address,undefined -fno-omit-frame-pointer)
fi
"${CC:-clang}" "${cflags[@]}" -c "$host/src/services/surface/raster.c" -o "$build/raster.o"
"${CC:-clang}" "${cflags[@]}" -c "$host/src/core/wire.c" -o "$build/wire.o"
"${CXX:-clang++}" "${flags[@]}" "$app/tests/input_test.cpp" "$app/voxel_world.cpp" "$app/voxel_render.cpp" "$app/voxel_mesher.cpp" "$sdk"/src/*.cpp -o "$build/input"
"${CXX:-clang++}" "${flags[@]}" -I"$host/include" "$app/tests/session_test.cpp" "$app/voxel_world.cpp" "$app/voxel_render.cpp" "$app/voxel_mesher.cpp" "$sdk"/src/*.cpp "$build/raster.o" "$build/wire.o" -o "$build/session"
"$build/session"
"${CXX:-clang++}" "${flags[@]}" "$app/tests/layout_test.cpp" -o "$build/layout"
"$build/layout"
"$build/input"
"${CXX:-clang++}" "${flags[@]}" "$app/tests/world_test.cpp" "$app/voxel_world.cpp" -o "$build/world"
"$build/world"
"${CXX:-clang++}" "${flags[@]}" -I"$host/include" "$app/tests/render_test.cpp" \
    "$app/voxel_world.cpp" "$app/voxel_mesher.cpp" "$sdk"/src/*.cpp \
    "$build/raster.o" "$build/wire.o" -o "$build/render"
"$build/render"
"${CC:-clang}" "${cflags[@]}" -DVOXEL_BENCH_SCENE=1 -c "$app/../voxel-craft/game.c" -o "$build/benchmark-c.o"
"${CXX:-clang++}" "${flags[@]}" -DVOXEL_BENCH_SCENE=1 "$app/tests/benchmark_test.cpp" \
    "$app/voxel_world.cpp" "$build/benchmark-c.o" -o "$build/benchmark"
"$build/benchmark"
"${CC:-clang}" "${cflags[@]}" -Wno-unknown-attributes -I"$root/deps/pxa-system/sdk/guest-c/include" \
    "$app/../voxel-craft/tests/raster_uv_test.c" "$app/../voxel-craft/game.c" \
    "$app/../voxel-craft/render.c" "$app/../voxel-craft/block_textures.c" -lm -o "$build/c-uv"
"$build/c-uv"
printf 'Voxel C++ input/world/render/benchmark tests OK\n'
