#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
output=${1:?Usage: sound_cache.sh /absolute/ignored/artifact-directory}
mkdir -p "$output"
cc -O2 -DPXA_SOUND_CACHE_BYTES=16384 -I"$root/cdogs-sdl" \
  -I"$root/common/recrof" -I"$root/common/sdl-games/vendor/sdl-mixer/include" \
  -I"$root/../../deps/pxa-system/sdk/guest-c/include" $(sdl2-config --cflags) \
  "$root/cdogs-sdl/sound_cache_pxa.c" "$root/tests/sdl-games/sound_cache.c" \
  $(sdl2-config --libs) -o "$output/sound-cache-test"
"$output/sound-cache-test" "$output"
