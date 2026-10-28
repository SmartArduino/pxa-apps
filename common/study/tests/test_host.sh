#!/usr/bin/env bash
set -euo pipefail
study_tests="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
study_build="$(mktemp -d)"
trap 'rm -rf "$study_build"' EXIT
"${CXX:-c++}" -std=c++20 -Wall -Wextra -Werror -fsanitize=address,undefined \
    -fno-omit-frame-pointer "$study_tests/core_test.cpp" -o "$study_build/core_test"
"$study_build/core_test"
python3 "$study_tests/content_test.py"
"${CXX:-c++}" -std=c++20 -Wall -Wextra -Werror -fsanitize=address,undefined "$study_tests/reader_test.cpp" -o "$study_build/reader_test"
"$study_build/reader_test"
