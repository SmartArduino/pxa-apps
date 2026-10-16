#!/usr/bin/env bash
set -euo pipefail
app=$(cd "$(dirname "$0")/.." && pwd)
cache=${PXA_SCRIPT_TEST_CACHE:-"${XDG_CACHE_HOME:-$HOME/.cache}/pxa-browser-js-tests"}
flags=(-O1 -g -fwrapv -fsanitize=address,undefined -fno-omit-frame-pointer -I"$app/vendor/quickjs")
compiler=${CC:-clang}
engine_flags=("${flags[@]}")
# Exercise the device's switch dispatcher and native nesting guard under ASan.
if [[ ${PXA_SCRIPT_TEST_WASI_MODEL:-0} == 1 ]]; then engine_flags+=(-D__wasi__=1); fi
if [[ ${PXA_SCRIPT_TEST_INTERPRETED:-0} == 1 ]]; then flags+=(-DPXA_SCRIPT_INTERPRETED=1); fi
fingerprint=$( { "$compiler" --version; printf '%s\n' -std=gnu11 "${engine_flags[@]}"; sha256sum "$app/vendor/quickjs/"*.[ch]; } | sha256sum)
cache="$cache/${fingerprint%% *}"
mkdir -p "$cache"
for name in quickjs dtoa libregexp libunicode cutils; do
    source="$app/vendor/quickjs/$name.c"
    if [[ ! -f "$cache/$name.o" || "$source" -nt "$cache/$name.o" ]]; then
        "$compiler" -std=gnu11 "${engine_flags[@]}" -D_GNU_SOURCE '-DCONFIG_VERSION="2026-06-04"' -c "$source" -o "$cache/$name.o"
    fi
done
cpp_flags=("${flags[@]}" -std=c++2c -fno-exceptions -fno-rtti -Wno-unused-parameter -Wno-missing-field-initializers)
cpp_compiler=${CXX:-clang++}
headers=$( { "$cpp_compiler" --version; printf '%s\n' "${cpp_flags[@]}"; sha256sum "$app/"*.hpp; } | sha256sum)
objects=()
for source in "$app/browser_script.cpp" "$app/browser_script_common.cpp" "$app/tests/script_test.cpp"; do
    digest=$( { printf '%s\n' "$headers"; sha256sum "$source"; } | sha256sum)
    object="$cache/${digest%% *}.o"
    if [[ ! -f "$object" ]]; then
        "$cpp_compiler" "${cpp_flags[@]}" -c "$source" -o "$object.tmp"
        mv "$object.tmp" "$object"
    fi
    objects+=("$object")
done
"$cpp_compiler" "${cpp_flags[@]}" "${objects[@]}" \
    "$cache"/{quickjs,dtoa,libregexp,libunicode,cutils}.o -lm -o "$cache/script-test"
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 "$cache/script-test" "$app/assets/browser-runtime.qjb"
