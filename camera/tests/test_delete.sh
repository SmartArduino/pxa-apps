#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
set -euo pipefail
if [[ $# != 4 ]]; then
  echo "Usage: $0 SDK_ROOT EXISTING_JPEG EXISTING_MKV ARTIFACT_DIR" >&2
  exit 2
fi
sdk_root="$(realpath "$1")"
jpeg_fixture="$(realpath "$2")"
mkv_fixture="$(realpath "$3")"
artifact_dir="$(realpath -m "$4")"
source_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
apps_root="$(cd "$source_dir/../.." && pwd)"
workspace_root="$(cd "$apps_root/../.." && pwd)"
mkdir -p "$artifact_dir"
# Keep assertions active regardless of any caller's Release environment.
# WASM import attributes have no meaning when testing the SDK on the host.
"${CXX:-clang++}" -std=c++2c -O1 -g -UNDEBUG -Wall -Wextra -Werror \
  -Wno-unknown-attributes -fsanitize=address,undefined -fno-omit-frame-pointer \
  -I"$sdk_root/sdk/guest-cpp/include" -I"$sdk_root/sdk/guest-c/include" \
  -I"$workspace_root/extensions/peripherals/sdk/include" \
  -I"$workspace_root/extensions/peripherals/include" \
  -I"$apps_root/xiaozhi/vendor/opus/include" \
  "$source_dir/gallery_delete_test.cpp" "$sdk_root/sdk/guest-cpp/src/runtime.cpp" \
  -o "$artifact_dir/gallery_delete_test"
"$artifact_dir/gallery_delete_test" "$jpeg_fixture" "$mkv_fixture"
