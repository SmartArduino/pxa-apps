#!/usr/bin/env bash
set -euo pipefail
app=$(cd "$(dirname "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
python3 - "$work" <<'PY'
from pathlib import Path
import sys,gzip,zlib
p=Path(sys.argv[1]);body=('<h1>中文压缩网页</h1><p>gzip deflate &amp; memory</p>'*800).encode()
for name,data in [('page.txt',body),('page.gz',gzip.compress(body,mtime=0)),('page.deflate',zlib.compress(body)),('empty.gz',gzip.compress(b'',mtime=0)),('bomb.gz',gzip.compress(b'x'*1048576,mtime=0))]:(p/name).write_bytes(data)
PY
flags=(-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -DZ_SOLO -DZ_TESTW=0)
for source in inflate inffast inftrees adler32 crc32; do
    "${CC:-clang}" "${flags[@]}" -c "$app/vendor/zlib/$source.c" -o "$work/$source.o"
done
"${CC:-clang}" "${flags[@]}" -c "$app/zlib_memory.c" -o "$work/memory.o"
"${CXX:-clang++}" "${flags[@]}" -std=c++2c -Wall -Wextra -Werror "$app/tests/content_encoding_test.cpp" "$work/"*.o -o "$work/test"
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 "$work/test" "$work"

sdk=${PXA_CPP_SDK_INCLUDE:-"$app/../../../deps/pxa-system/sdk/guest-cpp/include"}
"${CXX:-clang++}" "${flags[@]}" -std=c++2c -Wall -Wextra -Werror -Wno-unknown-attributes -I"$sdk" \
    -DPXA_COROUTINE_SLOT_COUNT=8 -DPXA_COROUTINE_SLOT_BYTES=2048 \
    "$app/tests/http_body_test.cpp" "$work/"*.o -o "$work/http-body-test"
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 "$work/http-body-test" "$work"
