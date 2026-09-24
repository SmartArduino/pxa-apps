#!/usr/bin/env bash
set -euo pipefail

app_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source_dir="$app_dir/assets/music"
target_dir="$app_dir/assets-esp32s3/music"
mkdir -p "$target_dir"

for track in theme sewers prison caves city halls; do
  ffmpeg -nostdin -hide_banner -loglevel error -y \
    -i "$source_dir/${track}_1.ogg" -map 0:a:0 -vn -sn -dn \
    -c:a libopus -b:a 24k -vbr on -ac 2 \
    "$target_dir/${track}_1.ogg"
done
