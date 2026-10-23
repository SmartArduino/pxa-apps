#!/usr/bin/env bash
set -euo pipefail
app_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
system="${PXA_SYSTEM_DIR:-$app_root/../../deps/pxa-system}"
tests="$app_root/tests/native"
build="$(mktemp -d "${TMPDIR:-/tmp}/pxa-app-native.XXXXXX")"
trap 'rm -rf "$build"' EXIT
flags=(-std=c++26 -O2 -fno-exceptions -fno-rtti -DPXA_GUEST_CPP_TRANSPORT=1
       -Wall -Wextra -Werror -Wno-attributes -Wno-missing-field-initializers
       -I"$system/sdk/guest-c/include" -I"$system/sdk/guest-cpp/include"
       -I"$system/sdk/testing" -I"$app_root" -I"$app_root/common"
       -include "$tests/export_callbacks.hpp")
for app in arcade garden-guard; do
    mkdir -p "$build/$app"
    python3 "$system/tools/i18n/compile_catalog.py" "$app_root/$app/i18n/messages.yaml" \
        "$app_root/$app/i18n/zh-CN.yaml" --output "$build/$app/pxa_app_messages.h"
done
if [[ "${SANITIZE:-0}" == 1 ]]; then
    flags+=(-g -fsanitize=address,undefined -fno-omit-frame-pointer)
    export UBSAN_OPTIONS=halt_on_error=1
fi
run_test() {
    local name="$1"; shift
    local catalog=arcade
    if [[ "$name $*" == *garden* ]]; then catalog=garden-guard; fi
    "${CXX:-clang++}" "${flags[@]}" -I"$build/$catalog" "$tests/$name.cpp" "$@" -o "$build/$name"
    "$build/$name"
    echo "$name OK"
}
run_test pxa_mapped_surface_test
run_test pxa_game_sfx_test
run_test pxa_maze_audio_test -I"$app_root/maze-evil" "$app_root/maze-evil/audio.cpp"
run_test pxa_maze_render_test -I"$app_root/maze-evil" \
    "$app_root"/maze-evil/{raster,palette,font,assets,sprites,raycast,world,render}.cpp
for module in flappy_bird jumping jump_jump plane_shooter brick_breaker tetris; do
    run_test pxa_canvas_smoke_test -DPXA_ARCADE_STANDALONE_TEST \
        "$app_root/arcade/modules/$module.cpp"
done
run_test pxa_canvas_smoke_test "$app_root/garden-guard/main.cpp"
for module in jumping plane_shooter brick_breaker; do
    run_test pxa_pointer_frame_pacing_test -DPXA_ARCADE_STANDALONE_TEST \
        "$app_root/arcade/modules/$module.cpp"
done
run_test pxa_tetris_controls_test -DPXA_ARCADE_STANDALONE_TEST "$app_root/arcade/modules/tetris.cpp"
run_test pxa_jump_jump_test -DPXA_ARCADE_STANDALONE_TEST
run_test pxa_garden_guard_test
run_test pxa_garden_guard_test -DGARDEN_DEBUG_UNLOCK_ALL=1
# App-local resource and parser regressions exercise the maintained C++ sources.
for entry in 'plane-shooter images_test images.cpp' 'store search_image_test search_image.cpp' \
             'store store_ticket_test store_client.cpp store_json.cpp' \
             'store store_account_test store_account.cpp store_client.cpp store_json.cpp' \
             'store store_utf8_test store_json.cpp' \
             'store store_media_test store_json.cpp store_media_url.cpp' \
             'store store_layout_test' \
             'store store_viewer_test' \
             'store store_size_test' \
             'weather weather_providers_test weather_providers.cpp'; do
    read -ra units <<< "$entry"
    app="${units[0]}"; name="${units[1]}"; sources=()
    for unit in "${units[@]:2}"; do sources+=("$app_root/$app/$unit"); done
    "${CXX:-clang++}" "${flags[@]}" -ffunction-sections -fdata-sections -Wl,--gc-sections \
        -I"$app_root/$app" "$app_root/$app/tests/$name.cpp" "${sources[@]}" -o "$build/$name"
    "$build/$name"
    if [[ "$name" == search_image_test ]]; then "$build/$name" stop-loading; fi
    echo "$name OK"
done
python3 "$system/tools/apps/check_ui.py" "$app_root"
echo 'PXA C++ app native regressions OK'
