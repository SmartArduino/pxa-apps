#!/bin/sh
# SPDX-License-Identifier: MIT
set -eu
app=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
out=${1:?usage: tests/run-native.sh /absolute/path/to/local/artifacts/topic/timestamp}
mkdir -p "$out"
flags='-DPXA_POCKET_UTILITY=1 -std=c++17 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined,float-cast-overflow -fno-omit-frame-pointer -fno-pie -no-pie'
${CXX:-c++} $flags "$app/tests/model_test.cpp" -o "$out/model-test"
"$out/model-test"
${CXX:-c++} $flags "$app/main.cpp" "$app/tests/runtime.cpp" "$app/tests/native.cpp" -o "$out/native"
"$out/native"
if [ "$#" -ge 2 ]; then
  sdk=$2
  ${CXX:-c++} $flags -Wno-attributes -I"$sdk/sdk/guest-c/include" "$app/tests/service_test.cpp" -o "$out/service-test"
  "$out/service-test"
fi
