# Original demo media

`demo-media.json` pins the original archive, its exact size and SHA-256, the
runtime layout, the original agreement, and required runtime file digests.
The Quake II recipe uses the 3.14 demo. Its enclosed limited-use demo license
allows free electronic distribution with the agreement. The data remains
proprietary; the engine's GPL does not license it. Do not sell this resource pack,
use it commercially, or include it with hardware sold to customers.

The resource package retains the complete original self-extracting archive under
`original/`, the original agreement under `licenses/`, and byte-for-byte runtime
files under `baseq2/`. No data converter, patch or retail data is used. Retaining
the complete original adds storage and download overhead; it does not add a
resident engine buffer. The game reads the installed resource volume in place.

The DOOM recipe distributes the original, unchanged episode-one shareware IWAD
once as `doom1.wad`, together with the original shareware terms and John Carmack's
redistribution clarification. The IWAD and notice are both pinned by SHA-256.
It contains no retail IWAD, PWAD or converted data. Its game-data permission is
independent of the engine's GPL. Select `doom` instead of `quake2` below to build
this resource package; the output is under `doom/`.

Build using the application's publisher key:

```sh
python3 local/pxa-apps/common/recrof/tools/build_demo_resources.py quake2 \
  --sdk-root deps/pxa-system \
  --output-root local/artifacts/game-resource-packs/<timestamp>/media \
  --signing-key /absolute/path/to/publisher-private.pem
```

`--archive` accepts an already downloaded original; its size and hash are still
checked. The generated `quake2/binding.json` supplies the exact `resources` field
for the application's source-format 0.2 `package.json`. Publish the signed
`quake2/resources.pxa` through the Store resource upload endpoint before publishing
all application variants with that binding. Use a new application release
sequence; change the resource generation only when the payload changes.

Store installs both parts in one operation. The system chooses or offers storage
volumes, validates both signatures and their binding, and provides a scoped media
root. Applications do not choose a filesystem path or require an SD card. App
updates that bind the same resource generation reuse installed media.

For C&C and Red Alert, `cnc-demo-media.json` pins the reviewed demo files. The
separate builder requires Python's `pycryptodome` to read original MIX indexes:

```sh
python3 local/pxa-apps/common/recrof/tools/build_cnc_resources.py red-alert \
  --input local/pxa-apps/.game-data/red-alert/files \
  --sdk-root deps/pxa-system \
  --output-root local/artifacts/game-resource-packs/<timestamp>/media \
  --signing-key /absolute/path/to/publisher-private.pem
```

Use `command-conquer` and its corresponding input directory for Tiberian Dawn.
Only the exact reviewed inputs are accepted. Tiberian Dawn excludes `map1.aud`
and `win1.aud`. Red Alert removes `SCORES.MIX` from `main.mix` and `INTRO.AUD` and
`MAP.AUD` from its nested `LOCAL.MIX`. Modified MIX indexes are rebuilt without
changing retained leaf assets. Original archives containing music are never
included. Each payload includes the EA non-affiliation notice and modding-policy
link. Distribution is free and non-commercial; music is excluded as required by
EA's current guidelines. The original artwork is not used for desktop icons.

Quake's Store resource package uses the first Lite level from LibreQuake
v0.09-beta. The archive is pinned in `librequake-media.json`. Artwork is
BSD-3-Clause; the generated `pop.lmp` is GPL-2.0. The resource package preserves
the original licenses and credits, and links to the fixed upstream source.
No id Software game data, music, demos or external `progs.dat` are included.
The engine uses its built-in GPL gamecode. This is a single-level free preview,
not the original Quake campaign.

```sh
python3 local/pxa-apps/common/recrof/tools/build_librequake_resources.py \
  --sdk-root deps/pxa-system \
  --output-root local/artifacts/game-resource-packs/<timestamp>/media \
  --signing-key /absolute/path/to/publisher-private.pem
```

The builder preserves model geometry and animation, limits palette-indexed skins
to 128×128, selects the first Lite map, and makes its exit start a new run. It
converts the result with the matching MCU format 3 tool. Quake 0.6 uses 10-bit
vertex indices, 21-bit triangle offsets and the `IDPL` model tag. Keep the signed
code and media binding together. Older MCU format 2 PAK files require conversion
again with the current tool. Other LibreQuake levels still require explicit
checks against the packed BSP, entity, string and working-buffer limits.

Duke3D uses the complete original 1.3D shareware archive. Its enclosed 1998
license allows free WWW distribution when all original files remain included
and unmodified. The runtime GRP and RTS are byte-for-byte copies. VOC samples
are read from their original segments, and type-one MIDI is merged only in
runtime memory. No converted data is distributed. Select `duke3d` with the
`build_demo_resources.py` command above.

OpenLara uses the complete original PC demo archive. Its original README
explicitly allows Internet distribution with every file included and unaltered.
The PHD levels and PCX title remain unchanged. The engine decodes the PCX at
runtime; the resource package does not contain GBA title assets, a GBA music
bank, or retail game data. Select `openlara` with the same builder. The generated
`levels.idx` is a platform catalog, separate from the original assets.

These original demo permissions are independent of the engines' GPL/BSD
licenses. Do not substitute converted Quake/Duke verification data or retail
Tomb Raider levels. The Quake shareware conversion is for local use with owned
data; it is not the Store recipe.

Original terms and publisher guidance:

- [Quake II source release: data retains its original terms](https://github.com/id-Software/Quake-2/blob/master/readme.txt)
- [DOOM shareware terms and redistribution clarification](https://github.com/redox-os/freedoom/blob/master/LICENSE-DOOM1)
- [Quake shareware agreement](https://github.com/id-Software/Quake/blob/master/WinQuake/data/SLICNSE.TXT)
- [LibreQuake project and redistribution permission](https://github.com/lavenderdotpet/LibreQuake)
- [C&C franchise modding guidelines](https://www.ea.com/games/command-and-conquer/news/modding-faq)
- [OpenLara engine repository](https://github.com/XProger/OpenLara)

Focused preparation checks:

```sh
python3 -m unittest discover -s local/pxa-apps/common/recrof/tests -p 'test_*resources.py'
```
