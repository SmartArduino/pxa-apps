#!/usr/bin/env bash
set -euo pipefail
app=$(cd "$(dirname "$0")/.." && pwd)
output=${1:?Pass an output directory under local/artifacts}
case "$output" in
  *local/artifacts/*) ;;
  *) echo 'Use an ignored local/artifacts output directory' >&2; exit 2 ;;
esac
mkdir -p "$output/native-objects"
for source in inflate inffast inftrees adler32 crc32; do
  "${CC:-cc}" -std=c11 -O1 -DZ_SOLO -fsanitize=address,undefined \
    -c "$app/vendor/zlib/$source.c" -o "$output/native-objects/$source.o"
done
"${CC:-cc}" -std=c11 -O1 -DZ_SOLO -fsanitize=address,undefined \
  -c "$app/zlib_memory.c" -o "$output/native-objects/zlib_memory.o"
"${CXX:-c++}" -std=c++17 -O1 -DZ_SOLO -Wall -Wextra -Werror \
  -fsanitize=address,undefined "$app/tests/native.cpp" "$app/codec.cpp" \
  "$output"/native-objects/*.o -o "$output/native-test"
"$output/native-test" "$app/assets/animations.bin"
"${CXX:-c++}" -std=c++17 -O1 -Wall -Wextra -Werror \
  -fsanitize=address,undefined "$app/tests/dsh_protocol.cpp" -o "$output/protocol-test"
"$output/protocol-test"
"${CXX:-c++}" -std=c++17 -O1 -Wall -Wextra -Werror -Wno-attributes \
  -I "$app/../../../deps/pxa-system/sdk/guest-c/include" \
  -fsanitize=address,undefined "$app/tests/http.cpp" -o "$output/http-test"
"$output/http-test"
"${CXX:-c++}" -std=c++17 -O1 -DZ_SOLO -Wall -Wextra -Werror -Wno-attributes \
  -I "$app/../../../deps/pxa-system/sdk/guest-c/include" \
  -fsanitize=address,undefined "$app/tests/animation.cpp" "$app/codec.cpp" \
  "$output"/native-objects/*.o -o "$output/animation-test"
"$output/animation-test" "$app/assets/animations.bin"
