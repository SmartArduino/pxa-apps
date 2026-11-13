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

"${CXX:-clang++}" "${flags[@]}" "$here/find_test.cpp" -o "$work/find-test"
"$work/find-test"

"${CXX:-clang++}" "${flags[@]}" "$here/compat_test.cpp" -o "$work/compat-test"
"$work/compat-test"
"${CXX:-clang++}" "${flags[@]}" "$here/text_mode_test.cpp" -o "$work/text-mode-test"
"$work/text-mode-test"

"${CXX:-clang++}" "${flags[@]}" "$here/forms_test.cpp" -o "$work/forms-test"
"$work/forms-test"

"${CXX:-clang++}" "${flags[@]}" "$here/css_test.cpp" -o "$work/css-test"
"$work/css-test"

"${CXX:-clang++}" "${flags[@]}" "$here/comic_test.cpp" -o "$work/comic-test"
"$work/comic-test"

"${CXX:-clang++}" "${flags[@]}" "$here/srcset_test.cpp" -o "$work/srcset-test"
"$work/srcset-test"

"${CXX:-clang++}" "${flags[@]}" "$here/encoding_test.cpp" -o "$work/encoding-test"
"$work/encoding-test" "$here/../assets/gb18030.bin"
