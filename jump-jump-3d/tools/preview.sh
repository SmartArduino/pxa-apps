#!/usr/bin/env bash
# Builds and runs the Jump Jump 3D offline render harness (tools/preview_render.c),
# which pushes the Guest draw list through the real Host raster kernel and
# writes PPM frames. Any extra arguments are forwarded to the harness.
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
app_dir="$(cd "$script_dir/.." && pwd)"
workspace_root="$(cd "$app_dir/../../.." && pwd)"
pxsys_root="${PXSYS_ROOT:-$workspace_root/deps/pxa-system}"
build_dir="${PXA_PREVIEW_BUILD:-/tmp/pxa-jump-jump-3d-preview}"
binary="$build_dir/preview_render"

mkdir -p "$build_dir"
cc_bin="${CC:-cc}"
"$cc_bin" -std=c11 -O2 -g -Wall -Wextra -Wno-attributes \
  -DJ3_SKIP_PROBE=1 \
  -I"$pxsys_root/sdk/guest-c/include" \
  -I"$pxsys_root/libpxa/include" \
  -I"$app_dir" \
  "$script_dir/preview_render.c" \
  "$app_dir/jump3d_render.c" \
  "$app_dir/jump3d_game.c" \
  "$app_dir/jump3d_palette.c" \
  "$app_dir/jump3d_font.c" \
  "$pxsys_root/libpxa/src/services/surface/raster.c" \
  "$pxsys_root/libpxa/src/core/wire.c" \
  -lm -o "$binary"
exec "$binary" "$@"
