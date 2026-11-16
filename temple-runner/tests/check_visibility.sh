#!/usr/bin/env bash
set -euo pipefail
app_dir="$(cd "$(dirname "$0")/.." && pwd)"
workspace_dir="$(cd "$app_dir/../../.." && pwd)"
: "${1:?Usage: check_visibility.sh PUBLIC_SDK_ROOT [ARTIFACT_DIR]}"
sdk_dir="$1/sdk/guest-cpp"
artifact_dir="${2:-$workspace_dir/local/artifacts/temple-runner/$(date +%Y%m%d-%H%M%S)}"
mkdir -p "$artifact_dir"
"${CXX:-clang++}" -std=c++2c -O2 -Wno-unknown-attributes -fsanitize=address,undefined \
  -I"$sdk_dir/include" "$app_dir/tests/visibility_test.cpp" \
  "$app_dir/renderer.cpp" "$app_dir/core.cpp" "$sdk_dir/src/game.cpp" \
  -o "$artifact_dir/visibility_test"
"$artifact_dir/visibility_test" | tee "$artifact_dir/visibility.log"
