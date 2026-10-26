#!/bin/sh
# Run from any directory; choose an ignored output directory explicitly.
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
output=${1:?Usage: gl_adapter.sh /absolute/ignored/artifact-directory}
mkdir -p "$output"
shared="$root/common/sdl-games"
sdk="$root/../../deps/pxa-system/sdk/guest-c/include"
# SDL compiler flags intentionally expand into separate arguments.
cc -O2 -I"$shared/include" -I"$root/common/recrof" -I"$sdk" \
  $(sdl2-config --cflags) "$shared/gl.c" "$root/tests/sdl-games/gl_adapter.c" \
  $(sdl2-config --libs) -lm -o "$output/gl-adapter-test"
"$output/gl-adapter-test"
