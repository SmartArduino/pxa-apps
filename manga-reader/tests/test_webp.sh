#!/usr/bin/env bash
set -euo pipefail
source_dir=$(cd -- "$(dirname -- "$0")/.." && pwd)
output_dir=$1
shift
mkdir -p "$output_dir"
objects=()
for file in "$source_dir"/vendor/libwebp/src/{dec,dsp,utils}/*.c "$source_dir"/webp_alloc.c "$source_dir"/vendor/picojpeg/picojpeg.c "$source_dir"/vendor/zlib/{deflate,trees,adler32,zutil}.c; do
    object="$output_dir/$(basename "${file%.c}").o"
    definitions=(-DHAVE_CONFIG_H=1 -DZ_SOLO -DNO_GZIP)
    if [[ "$file" == */utils/utils.c ]]; then
        definitions+=(-Dmalloc=manga_webp_malloc -Dcalloc=manga_webp_calloc -Dfree=manga_webp_free)
    fi
    clang -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -I"$source_dir/webp_config" -I"$source_dir/vendor/libwebp" "${definitions[@]}" -c "$file" -o "$object"
    objects+=("$object")
done
clang++ -std=c++20 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer "$source_dir/tests/webp_compat_test.cpp" "${objects[@]}" -o "$output_dir/test-webp"
"$output_dir/test-webp" "$@"
