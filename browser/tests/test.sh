#!/usr/bin/env bash
set -euo pipefail
here=$(cd "$(dirname "$0")" && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
flags=(-std=c++2c -O1 -g -Wall -Wextra -Wpedantic -Werror)
sdk=${PXA_CPP_SDK_INCLUDE:-"$here/../../../../deps/pxa-system/sdk/guest-cpp/include"}
flags+=(-I"$sdk" -Wno-unknown-attributes)
if [[ ${PXA_TEST_SANITIZERS:-1} == 1 ]]; then
    flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer)
fi
"${CXX:-clang++}" "${flags[@]}" "$here/core_test.cpp" -o "$work/core-test"
"$work/core-test"

"${CXX:-clang++}" "${flags[@]}" "$here/encoding_test.cpp" -o "$work/encoding-test"
"$work/encoding-test" "$here/../assets/gb18030.bin"
