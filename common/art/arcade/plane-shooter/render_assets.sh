#!/usr/bin/env sh
set -eu

art_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
app_root=$(CDPATH= cd -- "$art_dir/../../../.." && pwd)
assets_dir="$app_root/arcade/assets/plane-shooter"

render() {
    magick -background none "$art_dir/$1.svg" -strip "$assets_dir/$2.png"
}

render player-plane player-plane
magick -background none "$art_dir/player-plane.svg" -flop -strip \
    "$assets_dir/player-plane-left.png"
render enemy-plane enemy-plane
render enemy-dart enemy-dart
render enemy-fighter enemy-fighter
render enemy-cruiser enemy-cruiser
render enemy-gunship enemy-gunship
render boss-dreadnought boss-dreadnought
render pickup-energy pickup-energy
render pickup-shield pickup-shield
render pickup-overdrive pickup-overdrive
