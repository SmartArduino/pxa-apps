#!/usr/bin/env bash
set -euo pipefail
app=$(cd "$(dirname "$0")/.." && pwd)
sdk=${PXA_CPP_SDK_ROOT:-"$app/../../../deps/pxa-system/sdk/guest-cpp"}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
mkdir -p "$work/wasi"
# Only the entropy import is substituted. Protocol codecs use the real SDK.
cat > "$work/wasi/api.h" <<'HEADER'
#pragma once
#include <stddef.h>
#ifdef __cplusplus
extern "C"
#endif
unsigned short __wasi_random_get(unsigned char*,size_t);
HEADER
flags=(-std=c++2c -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer
       -ffunction-sections -fdata-sections -Wno-unknown-attributes -I"$sdk/include")
"${CXX:-clang++}" "${flags[@]}" "$app/tests/script_wire_test.cpp" -o "$work/wire"
"${CXX:-clang++}" "${flags[@]}" -I"$work" "$app/tests/script_client_test.cpp" \
    "$app/browser_script_client.cpp" "$app/browser_script_common.cpp" "$sdk/src/ipc.cpp" \
    -Wl,--gc-sections -o "$work/client"
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 "$work/wire"
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 "$work/client"
