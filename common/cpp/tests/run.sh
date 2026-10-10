#!/usr/bin/env bash
set -euo pipefail
app_root="$(cd "$(dirname "$0")/../../.." && pwd)"
pxa_root="${PXA_SYSTEM_DIR:-$app_root/../../deps/pxa-system}"
build="${PXA_CPP_GAME_TEST_BUILD:-$app_root/../cpp-game-tests}"
cmake -S "$app_root/common/cpp/tests" -B "$build" -DCMAKE_BUILD_TYPE=Release -DPXA_SYSTEM_DIR="$pxa_root" "$@"
cmake --build "$build" --parallel
ctest --test-dir "$build" --output-on-failure
