#!/usr/bin/env bash
set -euo pipefail
app_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
system="${PXA_SYSTEM_DIR:-$app_root/../../deps/pxa-system}"
reference="$app_root/common/cpp/tests/reference/c"
tests="$app_root/tests/native/reference"
build="$(mktemp -d "${TMPDIR:-/tmp}/pxa-app-reference.XXXXXX")"
trap 'rm -rf "$build"' EXIT
flags=(-std=c11 -O2 -Wall -Wextra -Werror -Wno-attributes
       -I"$system/sdk/guest-c/include" -I"$system/sdk/testing" -I"$app_root/common")
run() {
    local name="$1"; shift
    "${CC:-cc}" "${flags[@]}" "$tests/$name.c" "$@" -lm -o "$build/$name"
    "$build/$name"
}
for name in surface_ownership quality_controller; do
    run "pxa_voxel_${name}_test" -I"$reference/voxel-craft"
done
for name in raster_pipeline ui_overlay; do
    run "pxa_voxel_${name}_test" -I"$reference/voxel-craft" \
        "$reference/voxel-craft/block_textures.c" "$reference/voxel-craft/voxel_raster.c"
done
run pxa_jump3d_audio_test -I"$reference/jump-jump-3d" "$reference/jump-jump-3d/jump3d_audio.c"
run pxa_jump3d_game_test -I"$reference/jump-jump-3d" \
    "$reference/jump-jump-3d/jump3d_game.c" "$reference/jump-jump-3d/jump3d_palette.c"
echo 'Frozen C reference checks OK'
