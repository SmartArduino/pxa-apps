#!/bin/bash
set -xe

cp -r $1/*.json ./assets/maps
./utils/parse_tiled_maps.py ./assets/maps > ./map_segments.h