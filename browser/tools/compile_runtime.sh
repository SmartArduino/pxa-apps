#!/usr/bin/env bash
set -euo pipefail
app=$(cd "$(dirname "$0")/.." && pwd)
cache=${PXA_QUICKJS_NATIVE_CACHE:-"${XDG_CACHE_HOME:-$HOME/.cache}/pxa-browser-quickjs-20260604"}
flags=(-std=gnu11 -O2 -fwrapv -D_GNU_SOURCE '-DCONFIG_VERSION="2026-06-04"' -I"$app/vendor/quickjs")
compiler=${CC:-cc}
fingerprint=$( { "$compiler" --version; printf '%s\n' "${flags[@]}"; sha256sum "$app/vendor/quickjs/"*.[ch]; } | sha256sum)
cache="$cache/${fingerprint%% *}"
mkdir -p "$cache"
for name in quickjs dtoa libregexp libunicode cutils; do
    source="$app/vendor/quickjs/$name.c"
    if [[ ! -f "$cache/$name.o" || "$source" -nt "$cache/$name.o" ]]; then
        "$compiler" "${flags[@]}" -c "$source" -o "$cache/$name.o"
    fi
done
"$compiler" "${flags[@]}" "$app/tools/compile_runtime.c" "$cache"/{quickjs,dtoa,libregexp,libunicode,cutils}.o -lm -o "$cache/compile-runtime"
"$cache/compile-runtime" "$app/runtime/browser-runtime.js" "$app/assets/browser-runtime.qjb"
"$cache/compile-runtime" "$app/runtime/browser-select.js" "$cache/select-runtime.qjb" pxa-browser-select.js
python3 "$app/tools/compile_select_header.py" "$cache/select-runtime.qjb" "$app/browser_select_runtime.hpp"
"$cache/compile-runtime" "$app/runtime/browser-fragment.js" "$cache/fragment-runtime.qjb" pxa-browser-fragment.js
python3 "$app/tools/compile_select_header.py" "$cache/fragment-runtime.qjb" "$app/browser_fragment_runtime.hpp" fragment_runtime
python3 "$app/tools/compile_libraries.py" "$cache/compile-runtime"
(cd "$app/runtime"
    shopt -s nullglob
    inputs=(browser-runtime.js browser-select.js browser-fragment.js ../assets/browser-runtime.qjb ../browser_select_runtime.hpp ../browser_fragment_runtime.hpp
        ../vendor/quickjs/*.[ch] ../vendor/quickjs/VERSION
        ../tools/compile_runtime.c ../tools/compile_runtime.sh ../tools/compile_libraries.py ../tools/compile_select_header.py
        libraries.json ../browser_script_libraries.hpp ../assets/script-libraries/*
        ../vendor/es5-shim/*)
    sha256sum "${inputs[@]}" > compiled.sha256)
