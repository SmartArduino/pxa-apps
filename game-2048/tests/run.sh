#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
workspace="$(cd "$root/../../.." && pwd)"
out="${PXA_2048_TEST_OUTPUT:-$workspace/local/artifacts/mini-games/$(date +%Y%m%d-%H%M%S)/game-2048-tests}"
mkdir -p "$out"
cc="${CC:-cc}"
cxx="${CXX:-c++}"
flags=(-g -O1 -Wall -Wextra -Werror -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer -fno-pie -no-pie)
"$cc" -std=c99 "${flags[@]}" -c "$root/core.c" -o "$out/core.o"
"$cc" -std=c99 "${flags[@]}" "$root/tests/core_test.c" "$out/core.o" -o "$out/core_test"
"$out/core_test"
"$cxx" -std=c++17 "${flags[@]}" "$root/tests/app_test.cpp" "$root/main.cpp" "$out/core.o" -o "$out/app_test"
"$out/app_test"
sdk_headers="${PXA_SDK_HEADERS:-$workspace/deps/pxa-system/sdk/guest-c/include}"
"$cxx" -std=c++17 "${flags[@]}" -Wno-attributes -I"$sdk_headers" "$root/tests/audio_test.cpp" -o "$out/audio_test"
"$out/audio_test"
"$cxx" -std=c++17 "${flags[@]}" -Wno-attributes -I"$sdk_headers" -DPOCKET_AUDIO_GAME=1 \
  "$root/tests/game_pcm_test.cpp" "$root/main.cpp" "$out/core.o" -o "$out/2048_pcm_test"
"$out/2048_pcm_test"
"$cxx" -std=c++17 "${flags[@]}" -Wno-attributes -Wno-implicit-fallthrough -I"$sdk_headers" -DPOCKET_AUDIO_GAME=2 \
  "$root/tests/game_pcm_test.cpp" "$root/../pocket-chess/main.cpp" "$root/../pocket-chess/chess.cpp" -o "$out/chess_pcm_test"
"$out/chess_pcm_test"
"$cxx" -std=c++17 "${flags[@]}" -Wno-attributes -I"$sdk_headers" -DPOCKET_AUDIO_GAME=3 \
  "$root/tests/game_pcm_test.cpp" "$root/../pocket-sokoban/main.cpp" "$root/../pocket-sokoban/core.cpp" -o "$out/sokoban_pcm_test"
"$out/sokoban_pcm_test"
"$cc" -std=c99 -O1 -fsanitize=undefined -fno-sanitize-recover=all "$root/upstream/2048.c" -o "$out/upstream_test"
"$out/upstream_test" test
python3 - "$root" <<'PY'
from pathlib import Path
import hashlib, sys
root=Path(sys.argv[1])
expected={
    '2048.c':'90f794c675a696aa10980dd120d1f0503ee4ed33a61fef92c7df8f443f5b18a8',
    'LICENSE':'593783abd95546a2817d104b2656dd985a979a9e90da895729f211fc844b6402',
    'README.md':'bd8cc4d981eefdec443aa54cc4853aca470ca5b51dcd4c647c831dd887d70ff0'
}
for name,digest in expected.items():
    assert hashlib.sha256((root/'upstream'/name).read_bytes()).hexdigest()==digest, name
assert (root/'assets/LICENSE-2048.txt').read_bytes()==(root/'upstream/LICENSE').read_bytes()
upstream=(root/'upstream/2048.c').read_text()
expected_core=upstream[upstream.index('uint8_t findTarget('):upstream.index('void setBufferedInput(')]
expected_core=expected_core.replace('*score += 1 << array[t];','*score += array[t] < 32 ? UINT32_C(1) << array[t] : 0;')
actual=(root/'core.c').read_text()
assert actual[actual.index('uint8_t findTarget('):]==expected_core
print('2048 provenance: pinned originals and extracted rule bodies verified')
PY
