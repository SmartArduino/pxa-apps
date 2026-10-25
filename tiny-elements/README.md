# Tiny Elements

Tiny Elements 1.1.1, release sequence 3 (`pxa-tiny-elements`) is an original CC0 particle sandbox for
the PXA touch handheld. Its 72×48 world contains ten materials: empty/eraser,
sand, water, stone, wood, fire, steam, plant, seed and lava. The world uses 4 bits
per cell in storage. Sand falls through water, liquids flow, fire burns organic
matter, water extinguishes fire, steam rises and condenses, watered seeds grow
into plants, and water cools lava into stone and steam.

The bounded physics scan runs at 20 Hz. Each cell moves at most once per step;
an overdue tick runs at most two steps. Drawing uses the original palette,
small bright/shaded cell faces and deterministic flame sparks. Drawing never
changes the random generator or advances the world.

## Playing

Close the opening HELP card with its CREATE button or A/B. Hold a finger on the
world to keep adding particles; drag to draw a continuous interpolated stroke.
Lifting outside the world or cancelling the pointer releases the brush. Choose
materials along the bottom; the rightmost bottom button opens the other five
materials. The right rail contains pause, brush size, eraser, tasks, help and
clear. Brush labels S1/S2/S3 mean radii 0/1/3 cells, respectively.

D-pad moves the cursor. Hold A to paint, L/R change the selected allowed
material, B selects eraser, X pauses physics, Y changes brush size, and SELECT
opens the tasks. START or the upper-right menu button opens the shared menu
with Resume, Reset and Save and Exit. Reset, clear and scene replacement ask
for confirmation; CANCEL preserves the current world. Reset and scene changes
preserve all earned challenge badges. HELP and TASKS can be closed by B or a
touch in their bottom footer. Each challenge starts paused so that a solution
can be prepared before running physics.

There is no background music. Material selection, painting, clearing, reactions
and challenge badges retain distinct original CC0 synthesized cues. The help
panel's SOUND button (or Y while help is open) mutes these effects; the choice
is saved. Audio is
optional; drawing, physics and saving work without audio permission.

The pause menu selects 中文 / English independently of the world save. Chinese
glyphs derive from Noto Sans CJK under its SIL Open Font License; see
`../common/pocket-ui/OFL.txt`. The canvas fits the available screen while
preserving its aspect ratio, and touch coordinates follow the same viewport.

## Four challenges with touch-only solutions

All coordinates below are **logical 288×224 canvas coordinates**. The grid is
`(30,40,216,144)` and a grid-cell center is `(31+3*x,41+3*y)`. In the product
simulator or device, read `Pocket: viewport`: physical coordinates are
`origin_x + x * viewport_width / 288` and
`origin_y + y * viewport_height / 224`. Do not assume a fixed offset.
A stroke means DOWN at the first coordinate, MOVE to the second, then UP.
Repeated small moves are also accepted. No save editing is needed.

From a fresh launch, touch CREATE `(144,195)`. To enter a challenge, touch TASKS
`(267,123)`, choose the row listed below, then touch REPLACE `(85,180)`.
CANCEL is `(203,180)`. Scene selection supplies the required initial material;
brush size persists, so tap `(267,76)` until the indicated S label appears.
Finish the setup while FROZEN, then touch pause `(267,51)` once to start.

| Challenge | TASKS row | Setup before starting | Completion |
| --- | --- | --- | --- |
| Fill the jar | `(140,84)` | WATER, S3; stroke `(106,125)` → `(169,125)` | At least 60 water cells in the bottom of the jar; allow a few seconds for settling. |
| Grow a forest | `(140,113)` | WATER, S3; stroke `(64,137)` → `(214,137)` | At least 16 plant cells. The four preset seeds absorb water and grow; add more water if needed. |
| Hold the firebreak | `(140,142)` | STONE, S2; stroke `(145,77)` → `(145,167)` | Let 400 physics steps elapse, with at least 80 protected wood cells and at least 12 wood/plant cells burned elsewhere. |
| Cool the crucible | `(140,171)` | WATER, S1; draw three horizontal stripes `(103,155)` → `(175,155)`, `(103,161)` → `(175,161)`, `(103,167)` → `(175,167)` | At least 60 actual lava-to-stone reactions. Water replaces the stripe cells and reacts with adjacent hot lava after starting. |

The cooling challenge can also be approached by pouring water, erasing the
cooled stone crust and pouring again. Steam can later condense to water.
Forest/firebreak/crucible restrict materials to keep their goals meaningful;
disallowed choices do not replace the selected tool. Free creation and the jar
allow all ten materials. The task list marks earned badges with stars.

For an immediate natural sound trigger, close HELP and touch WATER `(137,199)`.
It selects water, plays the selection cue and logs `Elements: material selected`.
The new-badge log is `Elements: challenge badge earned`.

## Persistence and determinism

The explicit little-endian save is 1780 bytes, below the 2048-byte budget:
48-byte header, 1728-byte packed grid and a 4-byte FNV-1a checksum. It saves all
3456 materials, nonzero xorshift32 state, physics step, fractional accumulator,
scene timing/reaction counters, achievements, selected material, brush size,
physics pause, current overlay, pending confirmation, cursor and selection.
Held controller/touch contacts are released across lifecycle transitions.
Byte 43 stores the muted flag (0 or 1); bytes 44–47 remain reserved and zero.
Old TEL1 saves use zero at byte 43, so they retain enabled sound with no change
to the 1780-byte payload, version, world cells or chunked storage transport.

Loading checks magic/version/geometry, checksum, every reserved byte, every
material nibble, field ranges and the confirmation invariant before replacing
the live state. A rejected save leaves the live state intact. The simulation
uses no wall clock and rendering consumes no RNG. Pausing, saving and loading
therefore retain the exact future evolution for the same inputs and steps.

Saves are requested after a completed stroke, state/tool/overlay changes,
badges and every 15 seconds of active play. Physics does not write storage on
every step. The opt-in chunked runtime coalesces requests and flushes on
pause/exit. Its bounded exit flush can wait up to six seconds for the three
asynchronous storage operations and retries.

### Firmware storage transport

The 1780-byte game payload is unchanged. ESP firmware limits **each Storage
value to 960 bytes**, so Tiny links `../common/pocket-play/chunked-runtime.cpp`.
The chunked variant shares the same adaptive display, UTF-8 text, language
preferences, audio and lifecycle implementation as the default runtime.

The logical key `pxa-tiny-elements.save.v1` contains a 24-byte commit head.
All integers are explicit little endian, with no native struct layout:

| Byte offset | Size | Meaning |
| --- | --- | --- |
| 0 | 4 | ASCII `PPC1` |
| 4 | 1 | Transport version, exactly 1 |
| 5 | 1 | Committed bank, 0 or 1 |
| 6 | 2 | Total game payload bytes, 1 through 1792 |
| 8 | 4 | FNV-1a-32 of the complete game payload |
| 12 | 8 | Nonzero unsigned 64-bit commit generation |
| 20 | 4 | Reserved, all zero |

Each bank stores raw payload slices in
`pxa-tiny-elements.save.v1.b{bank}.c{index}`. Chunk zero is bytes 0 through 895
and chunk one is the remaining 884 bytes for the current 1780-byte save. The
transport supports at most two chunks of 896 bytes each and rejects larger
payloads. There are at most five keys; both Tiny banks plus the head contain
3584 value bytes in total.

A save takes an immutable snapshot, writes chunk zero and chunk one to the
opposite bank, waits for each successful acknowledgement, then writes the
commit head last. Only a successful head acknowledgement selects the new bank
for subsequent writes. Any failed submission or acknowledgement retries the
same snapshot from chunk zero after a one-second delay. Game changes during
that operation remain dirty for the next generation. This also handles a head
write that became durable before its acknowledgement failed: a retry writes
the same bytes, never a different world, into that bank.

Loading reads the head first, validates every field, reads only its named bank,
checks exact chunk lengths, total bytes and complete-payload FNV, and then
passes the reconstructed payload to the game's own atomic decoder. Missing,
truncated, mixed or corrupt chunks never reach the game. Uncommitted chunks
are ignored. The protocol relies on Storage v1's atomic whole-snapshot `set`
contract; interruption before the final head preserves the previous committed
world, or leaves the default world when there was no committed save.

## Build and verification

Run from the repository root. Choose an ignored artifact directory and keep
both temporary files and the build cache inside it. The release package
contains ESP32-S3, ESP32-S31 and Linux simulator AOT artifacts and a signature.
The optional device package keeps only S3 to reduce installed storage.

```sh
OUT=/absolute/path/to/local/artifacts/tiny-elements/your-run
mkdir -p "$OUT/tmp" "$OUT/cache"
TMPDIR="$OUT/tmp" PXA_BUILD_CACHE_DIR="$OUT/cache" \
  tools/app.sh build tiny-elements --source-root local/pxa-apps \
  --target esp32s3,esp32s31,simulator --aot-only --output "$OUT/package"
TMPDIR="$OUT/tmp" PXA_BUILD_CACHE_DIR="$OUT/cache" \
  tools/app.sh build tiny-elements --source-root local/pxa-apps \
  --target esp32s3 --aot-only --output "$OUT/device-package"
sh local/pxa-apps/tiny-elements/tests/run-native.sh "$OUT/native"
SANITIZE=1 sh local/pxa-apps/tiny-elements/tests/run-native.sh "$OUT/sanitized"
python3 local/pxa-apps/tiny-elements/tests/integration.py \
  --simulator "$SIMULATOR" --package "$OUT/package/pxa-tiny-elements" \
  --publisher-key deps/pxa-system/apps/pxa/.dev-signing/publisher-public.der \
  --oracle "$OUT/native/resume" --output "$OUT/simulator"
```

`SIMULATOR` is an existing `pxsys_product_simulator` with control-socket
support. Use a fresh output directory for each integration run; the test does
not delete existing state. The native suite checks conservation, reactions,
all four reachable goals, an unprotected firebreak failure, deterministic
continuation, strict rejection including forged checksums, atomic load,
brush/stroke bounds, framebuffer canaries, held input and storage frequency.
It also compiles the production chunk Writer/Reader directly and checks power
loss at every commit stage, failed and ambiguously acknowledged writes,
immutable retries, bank selection after reboot, all chunk-size boundaries,
strict head/chunk rejection and randomized fault sequences across generations.
It builds the native deterministic-resume oracle used by integration.
The product integration test drives actual touch/controller events, earns all
four badges, reads and independently decodes actual PXKV generations and PPC1
head/chunk assemblies, verifies every stored value is at most 960 bytes, checks
byte-exact EXIT/relaunch and matching world pixels, then compares resumed
physics with the native oracle at the actual observed step. It never injects
or replaces a host save. Reports, screenshots and captured saves go only to
the selected artifact output directory.

## License and assets

Game-specific source, rules, scenes, graphics, original geometric icons,
synthesized tones, tests and documentation are CC0-1.0. The source dedication
is in LICENSE; the full legal text and provenance notice are in
assets/LICENSE-CC0.txt and assets/NOTICE.txt. No downloaded art, music, maps or
game engine are used. Regenerate icon.png with assets/generate-icon.py and
Pillow. The separate common pocket-play runtime supplies the PXA lifecycle,
clipped drawing, audio and its pixel font. Tiny uses its isolated CC0 chunked
storage variant; the original shared runtime files are kept unchanged.
