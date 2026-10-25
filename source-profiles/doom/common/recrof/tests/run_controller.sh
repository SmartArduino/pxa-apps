#!/usr/bin/env bash
set -euo pipefail
tests="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
root="$(git -C "$tests" rev-parse --show-toplevel)"
sdk="${PXA_CONTROLLER_TEST_SDK:-$root/../../deps/pxa-system/sdk/guest-c/include}"
build="$(mktemp -d)"
trap 'rm -rf "$build"' EXIT
"${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Wno-attributes \
  -fsanitize=address,undefined -fno-omit-frame-pointer -g \
  -I"$tests/.." -I"$sdk" "$tests/../controller.c" \
  "$tests/controller_test.c" -o "$build/controller"
"$build/controller"
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -DPORT_RTS=1 \
  -fsanitize=address,undefined -fno-omit-frame-pointer -g \
  -I"$tests/rts-input-stubs" -I"$tests/.." \
  "$tests/../../../command-conquer/input.cpp" "$tests/rts_input_test.cpp" \
  -o "$build/rts-input"
"$build/rts-input"
"${CC:-cc}" -std=c11 -Wall -Wextra -Werror \
  -fsanitize=address,undefined -fno-omit-frame-pointer -g \
  "$tests/duke_keymap_test.c" -o "$build/duke-keymap"
"$build/duke-keymap"
"${CC:-cc}" -std=c11 -Wall -Wextra -Werror \
  -fsanitize=address,undefined -fno-omit-frame-pointer -g \
  -I"$tests/.." "$tests/pointer_motion_test.c" -lm -o "$build/pointer-motion"
"$build/pointer-motion"
