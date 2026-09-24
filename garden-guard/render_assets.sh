#!/usr/bin/env sh
set -eu

app_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
app_root=$(CDPATH= cd -- "$app_dir/.." && pwd)
plant_svg="$app_root/common/art/garden-extra-actors.svg"
zombie_svg="$app_root/common/art/garden-extra-zombies.svg"
assets_dir="$app_dir/assets"
work_dir=$(mktemp -d "${TMPDIR:-/tmp}/garden-assets.XXXXXX")
trap 'rm -rf "$work_dir"' EXIT HUP INT TERM

rsvg-convert -w 224 -h 32 -o "$work_dir/plants.png" "$plant_svg"
rsvg-convert -w 88 -h 48 -o "$work_dir/zombies.png" "$zombie_svg"

names="puff-shroom sun-shroom spikeweed fume-shroom doom-shroom magnet-shroom gloom-shroom"
index=0
for name in $names; do
    x=$((index * 32))
    magick "$work_dir/plants.png" -crop "32x32+${x}+0" +repage -strip \
        "$assets_dir/$name.png"
    index=$((index + 1))
done
magick "$work_dir/zombies.png" -crop 40x48+0+0 +repage -strip \
    "$assets_dir/pole-zombie.png"
magick "$work_dir/zombies.png" -crop 48x48+40+0 +repage -strip \
    "$assets_dir/gargantuar.png"

# The first 352 pixels are the stable original actors and shovel. Re-cropping
# them makes the script idempotent even when actors.png is already expanded.
magick "$assets_dir/actors.png" -crop 352x32+0+0 +repage \
    "$work_dir/base.png"
magick "$work_dir/plants.png" -background none -gravity west -extent 352x32 \
    "$work_dir/plant-row.png"
magick "$work_dir/zombies.png" -background none -gravity west -extent 352x48 \
    "$work_dir/zombie-row.png"
magick "$work_dir/base.png" "$work_dir/plant-row.png" "$work_dir/zombie-row.png" -append -strip \
    "$assets_dir/actors.png"
