#!/usr/bin/env bash
set -euo pipefail
tests="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
workspace="$(cd "$tests/../../../../.." && pwd)"
out="${1:-$workspace/local/artifacts/milkyfrog-games/$(date +%Y%m%d-%H%M%S)/native}"
mkdir -p "$out"
cxx="${CXX:-c++}"
flags=(-DPXA_POCKET_SYSTEM_TEXT=1 -std=c++17 -g -O1 -Wall -Wextra -Wno-misleading-indentation -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer -fno-pie -no-pie)
"$cxx" "${flags[@]}" "$tests/rules.cpp" -o "$out/rules"
"$out/rules"
"$cxx" "${flags[@]}" "$tests/layout.cpp" -o "$out/layout"
"$out/layout"
"$cxx" "${flags[@]}" -Wno-attributes -I"$workspace/deps/pxa-system/sdk/guest-c/include" "$tests/audio.cpp" -o "$out/audio"
"$out/audio"
for app in milkyfrog-2048 milkyfrog-rain milkyfrog-miner; do
    "$cxx" "${flags[@]}" -Wno-attributes -I"$workspace/deps/pxa-system/sdk/guest-c/include" "$tests/app.cpp" "$tests/../../../$app/main.cpp" -o "$out/$app"
    "$out/$app"
done
