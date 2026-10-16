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
(cd "$app/runtime" && sha256sum browser-runtime.js ../assets/browser-runtime.qjb \
    ../vendor/quickjs/*.[ch] ../vendor/quickjs/VERSION \
    ../tools/compile_runtime.c ../tools/compile_runtime.sh > compiled.sha256)
