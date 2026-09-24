# Pixel Dungeon (PXA)

A turn-based roguelike for PXA devices, built from Shattered Pixel Dungeon's
own art, audio and gameplay structure. It draws through the GameRender raster
service, so the whole game runs as a sandboxed WASM/AOT Guest App.

Original Shattered Pixel Dungeon assets and derived code are distributed under
GPL-3.0; the upstream license text is included in `LICENSE.txt`.

## What is original

| Area | Source |
|---|---|
| Tilesets | SPD `environment/tiles_{sewers,prison,caves,city,halls}.png` |
| Terrain features | SPD `environment/terrain_features.png`, `water*.png` |
| Hero, mobs, items | SPD `sprites/*.png`, `sprites/items.png` |
| Interface | SPD `interfaces/chrome.png`, `banners.png`, `toolbar.png`, `icons.png`, `sprites/avatars.png` |
| App icon | SPD `metadata/en-US/images/icon.png` |
| Title backdrop | SPD `splashes/title/archs.png` |
| Title flames | SPD `effects/fireball-short.png` |
| Sound effects | SPD `sounds/*.mp3` |
| Music | Uncut SPD `music/{theme,sewers,prison,caves,city,halls}_1.ogg` |
| Floors | One SPD region per five depths: sewers, prison, caves, city, halls |
| Walls | Original raised faces, 16-way interior joins, 4-way overhangs and door lintels for every region |

The art and packaged PCM sound-effect files are extracted by
`tools/generate_assets.py` and `tools/generate_audio.py`. No audio samples are
embedded in the executable. Simulator packages contain the six original Ogg
Vorbis tracks. For ESP32-S3, `tools/generate_device_music.sh` produces
full-length Ogg Opus versions under `assets-esp32s3/music/` to fit the
LittleFS install's temporary container and unpacked files without dropping
the original tracks or any other installed applications.
Device packages keep the target-specific AOT executable rather than a second
WASM copy, reducing peak LittleFS usage while the signed container is unpacked.

## Fetch the upstream assets

```sh
tools/fetch_spd_assets.sh /tmp/pxa-spd-assets      # sparse clone
python3 tools/generate_assets.py --spd-assets /tmp/pxa-spd-assets
python3 tools/generate_audio.py  --spd-assets /tmp/pxa-spd-assets
python3 tools/generate_text.py                     # strings + font atlases
mkdir -p assets/music
cp /tmp/pxa-spd-assets/core/src/main/assets/music/{theme,sewers,prison,caves,city,halls}_1.ogg assets/music/
tools/generate_device_music.sh
```

`generate_text.py` renders the anti-aliased ASCII atlas and CJK atlas pages
from the characters actually used by `tools/strings.py`. PXA limits each
texture to 256×256, so extra Chinese glyphs use a second texture slot rather
than reducing the translated text.

## Build and run

```sh
tools/app.sh build pixel-dungeon --target simulator
tools/simulator.sh product --profile pai-touch \
    --package local/app-output/pai-touch/pxa-pixel-dungeon \
    --publisher-key local/pxa-apps/.dev-signing/publisher-public.der

tools/app.sh build pixel-dungeon --board pai-touch --target esp32s3
pxadb package install local/app-output/pai-touch/pxa-pixel-dungeon.pxa \
    --port /dev/ttyACM0 --yes
```

The render size is negotiated from the actual display at startup and rebuilt
after a resolution/inset change. Wide 800×480 displays use a 2× pixel scale;
smaller panels render at 1×. Layout, input mapping and safe insets all use the
negotiated dimensions. The WASM guest stays within the pinned 1 MB memory cap.
The separate world zoom is 1×–3×: open the in-game pause menu and settings to adjust
it, or pinch with two fingers on devices with multi-touch support. It changes
tiles and actors, never the HUD, and is remembered across launches. The PXADB
desktop simulator currently advertises one pointer; use pause → settings
there to check zoom. The simulator's SDL audio sink plays the original effects
and region music when a system audio output device is available.
Drag the map to move the camera smoothly by pixels without moving the hero.
At every floor edge, drag farther to bring tiles out from under HUD controls;
the map and the controls zoom independently.
The HUD shows experience as a progress bar; tap the portrait for the exact
experience value and the amount needed for the next level.

## Gameplay

* Procedurally generated floors: neighbouring rooms form a connected route
  with extra loops, doors and clustered water/grass. Statues, alchemy
  pots, chests, animated water with original shore joins, two grass variants,
  raised grass, embers and traps.
* Normal steps have no dust burst; walking through tall grass tramples it to
  short grass, releases leaves and plays the original grass sounds.
* Turn-based movement and combat with accuracy, damage, armour and XP.
* The XP bar shows the full fraction when it fits legibly, otherwise the
  current XP alone; tap the portrait for the full current/next-level value.
  One-, two- and three-digit levels stay centered inside the badge.
* Hunger uses the original warning/starvation buff icons and messages. Tap the
  portrait for remaining satiety out of 320; the original shows the status
  icon and its description, but not a numeric satiety meter.
* Ten mobs taken from the original (rat, gnoll, crab, skeleton, bat, snake,
  spinner, slime, golem, Yog-Dzewa) with hunting AI, wake-on-sight and
  wander-when-idle.
* Loot: potions, scrolls, weapons and armour with upgrade levels, gold, food.
* Five independent save slots; the old `pixel-dungeon.save` key remains slot 1.
  On compact displays the save list pages in two or three touch-sized rows.
  Finished runs are recorded in a separate five-entry leaderboard.
* One iron-key door per new floor from depth 2 onward; its key appears next to
  the entrance and is consumed when that door opens. Existing legacy runs
  retain their original generator and map layout.
* A potion merchant appears on region-transition depths (6, 11, 16, 21),
  charging gold when the player chooses Buy.
* Hunger, natural regeneration, level-ups and a 25 floor descent ending in the
  Yog-Dzewa fight.
* Field of view with remembered terrain, auto-walk (tap a distant tile),
  drag-to-pan map, searching with a staggered blue scan effect and quick potion
  use. Search consumes two turns and extra hunger, finds visible nearby traps;
  the rogue checks a wider radius. Walking can passively notice traps.
* Class-specific hero frames switch to the original outfit for the equipped
  armor tier (0–4); the original status-panel art tracks the same outfit.
* Defeat keeps the final map visible beneath the original GAME OVER banner,
  with separate new-game and menu buttons.
* Progress is saved per slot through the Host key-value Storage service; floors are
  rebuilt from the run seed and replayed tile changes. A saved generator
  version keeps existing runs on their original floor layout.

## Controls

Tap a tile to walk or attack, tap the hero to wait. The bottom toolbar carries
the original's five actions: wait, search, potion, pack and stairs. A
controller's d-pad and A/B buttons work as well. Tap the portrait for hero
information; tap an item in the five-column pack to select it, then use or
drop it. Back opens the pause menu during play and dismisses overlays;
Back from the title page exits. The main menu offers five independent save
slots and hero selection, plus settings, rankings and the recent-message
journal. The HUD places level inside the portrait's original
badge; other character statistics remain in the hero information page.
The home actions use the original entrance, rankings, journal and settings
icons. Menu, close, save-slot and toolbar hit targets grow with larger display
profiles while preserving the original pixel-art proportions.
The status pane fits the health value inside its bar, using condensed pixel
digits when the small portrait frame cannot fit the full value. The experience
bar shows progress only; its value is in hero details. The level badge measures
one- and two-digit values before centering.

## Audio

The existing `pxa_audio_play_asset`/`pxa_audio_control_asset` ABI accepts a
package-relative `.ogg` path, loop flag, gain, pause/resume and stop commands.
The desktop product simulator decodes the complete Ogg Vorbis track (requires
`vorbisfile` development libraries), mixes it with 16 kHz PCM effects and
restarts only when the track ends. The title plays the original theme track;
region changes fade out the previous track and fade in the new track via
periodic gain commands. It validates that the
requested path resolves inside the package's `assets/` directory.

On the `pai-touch` ESP32 board the asset sink streams the packaged Ogg tracks
through the Espressif GMF audio codec's Vorbis/Opus decoder, resamples to
16 kHz mono and mixes them with the packaged 16 kHz PCM sound-effect files
before sending them to RPC701. Pause, resume, stop, looping and gain changes
are supported. Boards without a compatible asset sink play silently rather
than carrying duplicate PCM samples in the executable.

## Animations

Water and embers cycle through atlas frames, the hero plays walk frames while
moving, mobs idle/walk/attack/die through the original's sprite frames, ground
items sparkle, damage numbers float and remembered terrain is darkened with a
fixed-alpha painter overlay.

## Simplifications

* The wall joins, door orientations and overhangs follow the original map;
  region-specific branch decorations and the original's full fog occlusion
  overlay are not yet included (isolated hidden wall segments are culled).
* Menus and inventory use the original nine-patch art, but do not implement
  the full original's menus, talents or class descriptions.
* No wands or talents; the merchant and key system cover a subset of the
  original's shops, special locks and economy rather than the full item system.
* The journal currently shows recent in-session messages, not the original's
  full persistent notes, catalogues and landmarks.

## Tests

```sh
cd local/pxa-apps/pixel-dungeon
cc -O1 -fsanitize=address,undefined -Wno-attributes -I. \
   -I../../../deps/pxa-system/sdk/guest-c/include tools/selftest.c \
   game.c dungeon.c layout.c input.c font.c assets.c strings.c font_data.c \
   -o /tmp/pd-selftest && /tmp/pd-selftest
```
