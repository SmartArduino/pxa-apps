# Pet Garden

A small original pixel pet game for the PXA SDK 2.1. Pip lives in a warm room and a berry garden. Feed, wash, pet, play, rest, and decorate while helping Pip grow from a bud into a sprout and one of three adult forms. The 288×224 scene uses a shared readable Chinese 10×10 / ASCII 5×7 font, crisp original shapes, expressive eyes and ears, small bounces, care effects, and optional synthesized care sounds.

Audio consists only of short care, reward and minigame effects; there is no background music. The shared runtime silences audio during pause, background and exit. Audio availability does not change the pet save.

The interface defaults to Chinese. Open the shared START / top-right menu and choose Language to switch between Chinese and English; care, guides, dynamic rewards and pet names redraw immediately. The shared font uses 10×10 Chinese glyphs and 5×7 ASCII glyphs. Language is a runtime preference and does not change the game save format.

## Controls

| Place | Controller | Touch |
| --- | --- | --- |
| Home care | Left/right selects Feed, Wash, Pet, Play, Rest, Decor; A acts | Tap the matching bottom button |
| Pet affection | L/R gives a pat, or select Pet and A | Tap Pip |
| Room/garden | Up/down or SELECT toggles | Tap the left side of the scene |
| Pet position | Use touch | Tap an empty spot right of the left scene area |
| Minigame menu | X opens it; up/down chooses, A starts | Play, then a game card |
| Berry Catch | Hold left/right to move the basket | Touch and drag horizontally |
| Bloom Beat | A when the shrinking ring turns green | Tap the play area with the green ring |
| Decoration shop | Up/down selects, A buys or toggles | Tap an item row |
| Results | A repeats; B returns home | Tap Play Again or Home |
| Guide | Y; A/right advances, left returns a page; B closes | Tap the HELP button below the top-right menu, then tap to advance/close |
| Return home | B | Use the runtime menu's RESTART action |
| Pause / exit | START | Runtime menu at the top right |

The runtime reserves the top-right 24×20 pixels for its menu. It captures one actual pointer, sends MOVE/UP/CANCEL, and suppresses controller movement while paused or backgrounded. A cancelled gesture never triggers a care button. Touch actions activate on release; Bloom Beat uses the press edge for accurate timing.

## Care, growth and rewards

Pip starts with five berries, eight coins and four healthy needs. Needs decrease by one every 12 active seconds. They never cause death, lost growth, or evolution reversal. Time is foreground play time: leaving the app, opening its runtime menu, or turning off the device produces no offline neglect. A free snack grows every 30 active seconds, up to 20; both minigames also award two snacks, capped at 20.

Feed consumes one snack, adds up to 28 fullness and gives seven growth; it is available at fullness 85 or below and has a three-second cooldown. Wash adds up to 32 cleanliness and seven growth with a five-second cooldown; gentle grooming still builds a bond at full cleanliness. Pet adds nine joy and three growth with a three-second cooldown. Rest restores two energy each active second. Completing a nap from below full energy gives six growth; Rest again wakes Pip early. Pip can always play even when a need is low.

Sprout unlocks at **45 growth**, and adult bloom at **120 growth**. The adult branch follows the largest accumulated affinity when 120 is crossed; ties prefer care, then play, then affection. The adult choice stays fixed afterward, while growth can continue to 999.

| Adult | Affinity | Example reachable route from a new pet |
| --- | --- | --- |
| Mossbun | Feed and wash | Eighteen washes, five seconds apart: 126 growth in 85 active seconds |
| Sunfox | Completed minigames | Seven perfect Bloom Beat rounds: 126 growth in 78.4 active seconds |
| Moonmoth | Pats and completed naps | Forty pats, three seconds apart: 120 growth in 117 active seconds |

Ordinary mixed play typically takes a few minutes. The guide shows both thresholds and branch hints. Active foreground timers, not wall-clock timestamps, determine all these timings.

**Berry Catch** lasts 15 seconds. Catch falling pink berries for one point and golden berries for two. Misses have no penalty. Completion awards `2 + floor(score / 2)` coins and `10 + score` growth.

**Bloom Beat** presents eight blooms in 11.2 seconds. Hit within ±230 ms of each ring meeting the flower. The first active tap consumes a bloom, so repeated tapping cannot turn an early miss into a hit. Completion awards `2 + score` coins and `10 + score` growth. Both games restore joy, spend eight energy, keep a best score, and award a small participation reward even with zero points. Leaving a game before completion returns home without awarding it.

The decoration shop offers a bloom pot (6 coins), honey lantern (10), starry rug (14), and festival flags (18). Bought items remain owned and can be toggled for free. They appear in both the room and the garden.

**Reset means return home**, clearing an interrupted activity and waking the pet while preserving growth, adult form, needs, inventory, decorations, room, and pet position. It never silently creates a new pet. There is no erase-save command in the game.

## Persistence and automation

The shared `common/pocket-play` runtime manages asynchronous storage at `pxa-pet-garden.save.v1`. Real care, purchases, rewards, navigation changes and loads request a save; foreground state is also saved every five seconds. The runtime flushes on exit. Current view, needs, growth and affinities, inventory, decoration ownership/visibility, position, best scores, cooldowns, foreground timers, RNG seed and interrupted minigame objects are retained. Rest is resumable; result rewards are already credited and cannot be awarded again by reloading the result page.

The game payload is exactly 128 bytes: `PGD1` magic, format byte 1, three reserved zero bytes, explicit little-endian fields, reserved zero padding, then a 32-bit FNV-1a checksum over the first 124 bytes. It does not serialize a compiler-dependent structure. Length, magic, version, reserved bytes, checksum, enum/range limits, evolution thresholds, ownership relations and positions are validated before replacing live state. Unsupported or corrupt saves leave the current initialized pet intact. FNV is corruption detection, not cryptographic authentication.

Logs use the prefix `pet-garden` and concise action names such as `feed`, `wash`, `pet`, `evolve`, `buy`, `catch-start`, `bloom-start`, `finish`, `home`, `loaded`, or `rejected corrupt or unsupported save`. Action records include mode, growth, stage, branch, coins, snacks, four needs, rounds, and score. Modes are home=0, games=1, catch=2, rhythm=3, decor=4, help=5, result=6; adult branches are care=1, play=2, affection=3.

## Build and native tests

The app links `../common/pocket-play/runtime.cpp` and uses short care and minigame effects without background music; it does not modify or link the older pocket-game runtime. The package is `pxa-pet-garden` version 1.1.2, release sequence 4, with pinned maximum 1 MiB linear memory and a 64 KiB stack. The game uses fixed arrays and no heap allocation. Per tick work is bounded, and all rendering happens inside `draw()`. Optional `audio.playback` permission and audio service minimum 0.10 match the shared runtime's synthesized-tone support; the whole game works without audio.

Run package builds from the workspace root. Keep cache, temporary files and outputs in ignored local artifacts:

```sh
artifact_root="$PWD/local/artifacts/pocket-play/$(date +%Y%m%d-%H%M%S)/pet-garden"
mkdir -p "$artifact_root/cache" "$artifact_root/tmp"
PXA_BUILD_CACHE_DIR="$artifact_root/cache" TMPDIR="$artifact_root/tmp" \
  tools/app.sh build pet-garden --source-root local/pxa-apps \
  --target esp32s31,simulator,esp32s3 --aot-only \
  --output "$artifact_root/package"
```

Native tests compile the real model and game hooks against a local runtime stub. They exercise the actual progression routes, both minigames and their rewards, cooldowns, free harvest, rest, shop ownership, held movement, touch cancellation, safe reset, interrupted-game round trips, single-bit save corruption and recomputed-checksum invalid fields. The stub asserts drawing occurs only during `draw()` and that every text call fits the canvas. It writes PPM frames for visual inspection.

```sh
test_output="$PWD/local/artifacts/pocket-play/$(date +%Y%m%d-%H%M%S)/pet-tests"
mkdir -p "$test_output"
c++ -std=c++17 -Wall -Wextra -Werror -pedantic \
  -fsanitize=address,undefined -fno-omit-frame-pointer -g \
  local/pxa-apps/pet-garden/core.cpp \
  local/pxa-apps/pet-garden/main.cpp \
  local/pxa-apps/pet-garden/tests/runtime_stub.cpp \
  local/pxa-apps/pet-garden/tests/native.cpp -o "$test_output/native"
"$test_output/native" "$test_output" > "$test_output/native.log" 2>&1
```

## Provenance and license

All game code, creature designs, scene artwork, decorations, minigame art, synthesized sound patterns, icon and icon source are original and MIT licensed. There are no downloaded pet art packs, TamaFi assets, external sprite sheets, or sampled audio. See `LICENSE` and the redistribution notice in `assets/LICENSE.txt`. The separately shared runtime and its original 5×7 font are CC0-1.0.

The canonical 128×128 icon is generated by the self-contained Python standard-library source:

```sh
python3 local/pxa-apps/pet-garden/assets/icon.py
```
