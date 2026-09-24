#!/usr/bin/env bash
# Fetch the Shattered Pixel Dungeon art used by tools/generate_assets.py.
#
# Only the environment and sprite sheets are needed, so this uses a sparse,
# blob-filtered clone. The checkout is intentionally kept outside the app:
# generated C is checked in, the upstream art is not vendored.
set -euo pipefail

target="${1:-${PXA_SPD_ASSETS:-/tmp/pxa-spd-assets}}"
repo="https://github.com/00-Evan/shattered-pixel-dungeon.git"

if [[ -d "$target/.git" ]]; then
  echo "already fetched: $target"
  exit 0
fi
git clone --depth 1 --filter=blob:none --sparse "$repo" "$target"
git -C "$target" sparse-checkout set core/src/main/assets
echo "SPD assets ready at $target"
