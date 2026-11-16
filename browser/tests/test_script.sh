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
"$cpp_compiler" "${cpp_flags[@]}" "$app/tests/script_heap_test.cpp" -o "$cache/heap-test"
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 "$cache/heap-test"
"$cpp_compiler" "${cpp_flags[@]}" "$app/tests/script_memory_test.cpp" "${objects[@]:0:2}" \
    "$cache"/{quickjs,dtoa,libregexp,libunicode,cutils}.o -lm -o "$cache/memory-test"
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 "$cache/memory-test" "$app/assets/browser-runtime.qjb"
"$cpp_compiler" "${cpp_flags[@]}" "${objects[@]}" \
    "$cache"/{quickjs,dtoa,libregexp,libunicode,cutils}.o -lm -o "$cache/script-test"
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 "$cache/script-test" "$app/assets/browser-runtime.qjb"
"$cpp_compiler" "${cpp_flags[@]}" "$app/tests/script_event_test.cpp" "${objects[@]:0:2}" \
    "$cache"/{quickjs,dtoa,libregexp,libunicode,cutils}.o -lm -o "$cache/event-test"
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 "$cache/event-test" "$app/assets/browser-runtime.qjb"
"$cpp_compiler" "${cpp_flags[@]}" "$app/tests/script_event_options_test.cpp" "${objects[@]:0:2}" \
    "$cache"/{quickjs,dtoa,libregexp,libunicode,cutils}.o -lm -o "$cache/event-options-test"
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 "$cache/event-options-test" "$app/assets/browser-runtime.qjb"
"$cpp_compiler" "${cpp_flags[@]}" "$app/tests/script_click_layout_test.cpp" "${objects[@]:0:2}" \
    "$cache"/{quickjs,dtoa,libregexp,libunicode,cutils}.o -lm -o "$cache/click-layout-test"
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 "$cache/click-layout-test" "$app/assets/browser-runtime.qjb"
"$cpp_compiler" "${cpp_flags[@]}" "$app/tests/script_connection_test.cpp" "${objects[@]:0:2}" \
    "$cache"/{quickjs,dtoa,libregexp,libunicode,cutils}.o -lm -o "$cache/connection-test"
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 "$cache/connection-test" "$app/assets/browser-runtime.qjb"
"$cpp_compiler" "${cpp_flags[@]}" "$app/tests/script_snapshot_test.cpp" "${objects[@]:0:2}" \
    "$cache"/{quickjs,dtoa,libregexp,libunicode,cutils}.o -lm -o "$cache/snapshot-test"
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 "$cache/snapshot-test" "$app/assets/browser-runtime.qjb"
"$cpp_compiler" "${cpp_flags[@]}" "$app/tests/script_dom_batch_test.cpp" "${objects[@]:0:2}" \
    "$cache"/{quickjs,dtoa,libregexp,libunicode,cutils}.o -lm -o "$cache/dom-batch-test"
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 "$cache/dom-batch-test" "$app/assets/browser-runtime.qjb"
"$cpp_compiler" "${cpp_flags[@]}" "$app/tests/script_form_test.cpp" "${objects[@]:0:2}" \
    "$cache"/{quickjs,dtoa,libregexp,libunicode,cutils}.o -lm -o "$cache/form-test"
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 "$cache/form-test" "$app/assets/browser-runtime.qjb"
"$cpp_compiler" "${cpp_flags[@]}" "$app/tests/script_textarea_test.cpp" "${objects[@]:0:2}" \
    "$cache"/{quickjs,dtoa,libregexp,libunicode,cutils}.o -lm -o "$cache/textarea-test"
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 "$cache/textarea-test" "$app/assets/browser-runtime.qjb"
"$cpp_compiler" "${cpp_flags[@]}" "$app/tests/script_select_test.cpp" "${objects[@]:0:2}" \
    "$cache"/{quickjs,dtoa,libregexp,libunicode,cutils}.o -lm -o "$cache/select-test"
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 "$cache/select-test" "$app/assets/browser-runtime.qjb"
"$cpp_compiler" "${cpp_flags[@]}" "$app/tests/script_selector_test.cpp" "${objects[@]:0:2}" \
    "$cache"/{quickjs,dtoa,libregexp,libunicode,cutils}.o -lm -o "$cache/selector-test"
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 "$cache/selector-test" "$app/assets/browser-runtime.qjb"
"$cpp_compiler" "${cpp_flags[@]}" "$app/tests/script_text_mode_test.cpp" "${objects[@]:0:2}" \
    "$cache"/{quickjs,dtoa,libregexp,libunicode,cutils}.o -lm -o "$cache/text-mode-test"
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 "$cache/text-mode-test" "$app/assets/browser-runtime.qjb"
"$cpp_compiler" "${cpp_flags[@]}" "$app/tests/script_fragment_test.cpp" "${objects[@]:0:2}" \
    "$cache"/{quickjs,dtoa,libregexp,libunicode,cutils}.o -lm -o "$cache/fragment-test"
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 "$cache/fragment-test" "$app/assets/browser-runtime.qjb"
"$cpp_compiler" "${cpp_flags[@]}" "$app/tests/script_class_list_test.cpp" "${objects[@]:0:2}" \
    "$cache"/{quickjs,dtoa,libregexp,libunicode,cutils}.o -lm -o "$cache/class-list-test"
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 "$cache/class-list-test" \
    "$app/assets/browser-runtime.qjb" "$app/tests/class_list_cases.js" "$app/tests/class_list_expected.json"
"$cpp_compiler" "${cpp_flags[@]}" "$app/tests/script_dataset_test.cpp" "${objects[@]:0:2}" \
    "$cache"/{quickjs,dtoa,libregexp,libunicode,cutils}.o -lm -o "$cache/dataset-test"
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 "$cache/dataset-test" \
    "$app/assets/browser-runtime.qjb" "$app/tests/dataset_cases.js" "$app/tests/dataset_expected.json"
"$cpp_compiler" "${cpp_flags[@]}" "$app/tests/library_test.cpp" "${objects[@]:0:2}" \
    "$cache"/{quickjs,dtoa,libregexp,libunicode,cutils}.o -lm -o "$cache/library-test"
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 "$cache/library-test" \
    "$app/assets/browser-runtime.qjb" "$app/vendor/es5-shim/es5-shim.min.js"
python3 "$app/tests/library_generator_test.py" "$app/tools/compile_libraries.py"
# The precise parser cancellation/error contract uses PXA's WASI recursion
# guard. Native parsing intentionally retains upstream lookahead behavior.
if [[ ${PXA_SCRIPT_TEST_WASI_MODEL:-0} == 1 ]]; then
"$compiler" -std=gnu11 "${engine_flags[@]}" "$app/tests/compile_interrupt_test.c" \
    "$cache"/{quickjs,dtoa,libregexp,libunicode,cutils}.o -lm -o "$cache/compile-interrupt-test"
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 "$cache/compile-interrupt-test"
fi
"$compiler" -std=gnu11 "${engine_flags[@]}" "$app/tests/array_interrupt_test.c" \
    "$cache"/{quickjs,dtoa,libregexp,libunicode,cutils}.o -lm -o "$cache/array-interrupt-test"
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 "$cache/array-interrupt-test"
