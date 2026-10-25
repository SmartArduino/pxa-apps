# Crisp Arcade

A complete four-game PXA port of ABA Games’ MIT-licensed **crisp-game-lib-portable** C implementation. The original games, physics, collision checks, sprites, palette, particle effects and generated action sounds are compiled from unchanged upstream sources. The PXA launcher adds descriptions, controls, independent high scores, original game symbols and touch/controller support. The non-vendor bridge disables background music on every launch and restart while preserving upstream action effects.

The interface defaults to Chinese. Use Language in the shared START / top-right menu to switch to English, including launcher cards, controls, scores and game-over prompts. The draw layer replaces upstream ASCII-only score and game-over labels with shared Chinese 10×10 / ASCII 5×7 text; all vendored game bytes remain unchanged. Language is stored by the runtime separately from game progress.

Package: `pxa-crisp-arcade`, version `1.1.1`, release sequence `3`. Requires PXA SDK 2.1 and the sibling `common/pocket-play` runtime.

## Play

Select a card with the controller arrows and press **A**, or touch a card. Each game starts immediately. A button used to choose a card must first be released before it can affect the game. Touch anywhere below the header to perform the game’s action; keep touching to hold it. A captured touch continues through movement outside the game view, and releasing outside the canvas or cancelling the touch releases the action.

| Game | Original view | Action and goal |
| --- | --- | --- |
| **CAST N** | 150×100 | Hold A/touch to charge casting power; release to cast; tap to pull the line. Catch blue fish, avoid red fish, and keep ahead of the rising water. |
| **GRAPPLING H** | 100×150 | Tap to release the current hook. Release the input, then hold again near a red highlighted anchor to grab it. Swing upward before the screen scrolls past you. |
| **REFLECTOR** | 100×100 | Tap to reverse the UFO. Hold to strengthen the reflector and slow movement; return tank shots and chain explosions. |
| **COLOR ROLL** | 100×100 | Tap to shoot. The shot adopts the first bar’s color and clears matching bars; time shots to make longer chains. |

**B** or the top-left **HOME** button returns to the launcher. **START** or the reserved top-right runtime menu pauses, with continue, restart and exit actions. Runtime restart restarts the selected game without erasing records. On game over, release and press A/touch again after the original short delay to retry, or return home. The game-over screen stays visible until an intentional retry; the upstream attract-mode replay is disabled because the launcher supplies the titles and instructions.

No simulation runs in the launcher. The shared runtime stops ticks and drawing during pause/background and cancels captured pointers. Original 60 FPS timing is maintained with an integer accumulator, with at most six simulation steps per runtime tick and no background-time catch-up. Controller A uses the runtime’s continuous held state. A bounded 16-edge queue preserves quick press/release pairs between simulation frames.

## Rendering, sound and memory

The source view proportions are retained: CAST N is 264×176; GRAPPLING H is 120×180; REFLECTOR and COLOR ROLL are 180×180, centered below a separate header. No game or launcher text occupies the runtime’s top-right 24×20 menu area. The launcher uses the shared runtime’s original 5×7 font; in-game text/sprites use upstream patterns.

`backend.cpp` implements the exact declarations in upstream `machineDependent.h`. It converts RGB-byte rectangles and 6×6 RGB character grids into a fixed **15,000-pixel RGB565 buffer (30,000 bytes)**. The core draws and checks collisions during its simulation update, so this buffer is independent of the runtime framebuffer. `game::draw()` alone scales the buffer into `pocket::framebuffer()`; drawing never advances game state. Upstream black is a nonzero gray, while RGB zero in character grids denotes transparency.

The app requests optional `audio.playback` permission and `audio` service version 0.10. Upstream absolute audio timestamps are passed unchanged to `pocket::tone()` and its clock is `pocket::millis()/1000`. Permission denial leaves every game playable silently. The shared runtime bounds, mixes and silences its tone queue on pause/background.

The package pins linear memory to 1 MiB and a 64 KiB stack, including the runtime’s two 288×224 RGB565 display buffers. Game/adapter state and source pixels use fixed static storage; the adapter performs no heap allocation and requires no SDL or external graphics/audio library. `-fwrapv` preserves upstream signed hash wraparound. The upstream core is included once through `core_bridge.c`, whose small accessors select a game, inject the random seed/high score and expose its state. `registration.c` replaces upstream `menuGameList.c` with exactly these four registrations, avoiding references to unlinked games.

## Saves and automation

The runtime stores a **48-byte version-1 record** under `pxa-crisp-arcade.save.v1`: `CRSP` magic, format version, selected card, four high scores, four play counters, reserved zero bytes and an FNV-1a checksum. Values are little endian; scores and play counters saturate at 99,999,999. Load validates exact length, magic, version, all reserved fields, selection, every score/counter range and checksum before changing any state. Corrupt or unsupported data leaves current records intact.

High scores and selected card survive exit/relaunch. An interrupted in-progress round starts afresh; transient physics, pointers and random state are intentionally not serialized. The runtime coalesces asynchronous save requests and handles final persistence on exit. Runtime restart and returning home keep the records.

Automation can match the `crisp` log prefix and `launch`, `restart`, `home`, `score`, `game-over`, `save-restored`, `save-rejected` events. Launches include the game title; score/game-over events include score and best. No frame-by-frame logging is emitted.

## Source and licensing

Official upstream: <https://github.com/abagames/crisp-game-lib-portable>, `main` resolved and downloaded on 2026-10-24 (UTC+08) at commit **`7c318dc87f1f6fb8cc03449613d8fdba73b380e9`**. Sources are the portable C implementation that already supports M5Stack/ESP32-class boards, not a JavaScript reconstruction.

Pinned archive: <https://codeload.github.com/abagames/crisp-game-lib-portable/zip/7c318dc87f1f6fb8cc03449613d8fdba73b380e9>.

- Commit archive SHA-256: `e71023094d540365235b0a9cf26e495fd4ee6282a317675b1130a028c32f9d4a`.
- Downloaded main-branch archive SHA-256: `ab1df4b3d5b79db990d536e31799dfa863a8cdb89bca9b4271a83eade317abcf`.
- All 20 retained upstream files were compared byte-for-byte between those archives. Per-file SHA-256 hashes are in [vendor/provenance.json](vendor/provenance.json); native tests verify them before compilation.
- Vendored originals: four `src/games/game*.c` files, the necessary `src/lib` C core/headers (without the 15-game registry), and the complete root `LICENSE.txt`.
- ABA Games’ complete MIT license remains in [vendor/LICENSE.txt](vendor/LICENSE.txt) and is packaged as [assets/LICENSE-crisp-game-lib-portable.txt](assets/LICENSE-crisp-game-lib-portable.txt). Copyright (c) 2022 ABA Games.
- Adapter, launcher, test code, documentation and original geometric icon: MIT, Copyright (c) 2026 Crisp Arcade contributors. Full text: [LICENSE.txt](LICENSE.txt) and packaged [assets/LICENSE-crisp-arcade.txt](assets/LICENSE-crisp-arcade.txt).
- Shared runtime and launcher font: CC0-1.0, with full packaged [assets/LICENSE-CC0.txt](assets/LICENSE-CC0.txt).
- Packaged attribution summary: [assets/NOTICE.txt](assets/NOTICE.txt). Recreate the original icon with `python3 local/pxa-apps/crisp-arcade/tests/make_icon.py` from the workspace root.

Keep all vendored files unchanged on updates. Update the pinned commit, archive/file hashes, full license and this provenance section together. Put platform fixes in the adapter or bridge. The C compiler can emit pre-existing conservative warnings around upstream sound chord initialization and a CAST N fixed-width sprite row; the original files remain intact.

## Native checks

Run from the workspace root with Python 3 and native C/C++ compilers:

```sh
python3 local/pxa-apps/crisp-arcade/tests/run.py --sanitize
```

This compiles the actual upstream C files with the PXA adapter and a draw-only runtime stub. It does not build any PXA package. The tests assert real CAST N power/casting/pulling states, controller/touch holds, short taps, outside releases and cancellation; GRAPPLING H hook release/grab/hold; REFLECTOR turn/hold strength; COLOR ROLL shooting and real score generation; natural game-over in all four games; bounded 60 FPS accumulation; launcher inactivity; launch-button release gating; independent high scores, game-over persistence and save restoration; every-byte corruption, all short lengths, overlong/null data, checksummed range violations and score saturation; draw-only access and exclusion of the runtime menu rectangle. Same-translation-unit test probes include unchanged game files to inspect private state without changing production code.

Output defaults to ignored `local/artifacts/pocket-play/20261024-015231/crisp-tests/`, including native executable/object files and PNG screenshots. An explicit `--output` must also be within ignored `local/artifacts/`. The runner uses the shared runtime font only to make the native screenshots representative. Sanitizers are optional: omit `--sanitize` for a normal native run. Generated results are not source documentation.

## Package build

The integration owner builds and signs packages from the workspace root. Keep all outputs/caches in ignored artifacts:

```sh
artifact_root="$PWD/local/artifacts/pocket-play/$(date +%Y%m%d-%H%M%S)/crisp-build"
mkdir -p "$artifact_root/cache" "$artifact_root/tmp"
PXA_BUILD_CACHE_DIR="$artifact_root/cache" TMPDIR="$artifact_root/tmp" \
  tools/app.sh build crisp-arcade --source-root local/pxa-apps \
  --target esp32s31,simulator,esp32s3 --aot-only \
  --output "$artifact_root/package"
```

The shared Pocket Play runtime must exist before packaging. `assets/` contains the icon and complete notices needed in the signed package. Simulator/device integration validates the runtime’s actual surface scaling, pause/background lifecycle, optional audio and asynchronous storage; the native suite validates this app’s adapter and game behavior.
