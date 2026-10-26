#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
output=${1:?Usage: pic_cache.sh /absolute/ignored/artifact-directory}
mkdir -p "$output"
cc -O2 -I"$root/cdogs-sdl" $(sdl2-config --cflags) \
  "$root/cdogs-sdl/pic_cache_pxa.c" "$root/tests/sdl-games/pic_cache.c" \
  -Wl,--wrap=SDL_DestroyRenderer $(sdl2-config --libs) -o "$output/pic-cache-test"
"$output/pic-cache-test"
