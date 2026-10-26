# PXA Apps

[简体中文](README.md)

Application sources for the PXA workspace. The PXA runtime, Guest SDK, packaging
tools, and a minimal `hello` smoke-test app remain in `pxa-system`. All product
applications and their shared art live here.

ABI, IPC and resource lifecycle fixtures live under
`pxa-system/simulator/desktop/tests/apps/`. Run them with that repository's
`tools/package/test_core2_smoke.sh`, `test_core2_ipc_smoke.sh` and
`test_resource_app.sh`. Diagnostic apps, rendering benchmarks and experimental
game prototypes have been removed from this repository; historical validation
reports and screenshots are retained.

Applications use **Core 2.x**. In-house C++ applications build as **C++26**;
ported games retain their upstream C/C++ implementations.
The production C versions of Jump Jump, Pixel Dungeon and Voxel Craft have
been retired; use [`jump-jump-3d`](jump-jump-3d/README.md),
[`pixel-dungeon`](pixel-dungeon/README.md) and `voxel-craft`.
Their frozen C algorithms remain only under `common/cpp/tests/reference/c`
for timing, pixel and gameplay comparisons; they have no package manifests.

Application names and package IDs omit implementation-language suffixes. See
[the naming and existing-save notes](docs/app-names.zh-CN.md).

Callback-based applications use the optional, stateless `pxa::call` path via
`common/cpp/Application.cmake`. This reuses their bounded protocol helpers
and saved message IDs without allocating a Context, coroutine pool or request
table. Apps that use asynchronous tasks and declarative UI link the full C++
SDK. Their PXA imports are only `pxa.core.v2::pxa_call` and `pxa_io`;
WASI libc imports remain explicitly declared by each component.

The migration results, device measurements and coverage limits are recorded in
[the Core 2 / C++26 report](docs/core2-cpp-migration-20261011.zh-CN.md).

[`novel-reader`](novel-reader/README.md) is the C++ network and offline TXT
reader, with configurable JSON sources, system input, adaptive typography,
chapter navigation and automatic page turns. Its device measurements and
reproducible acceptance checks live in the application's `docs` and `tests`.

[`browser`](browser/README.en.md) is PXA Browser, a small-screen reading browser with streaming
HTTP/HTTPS, keyword search, links, history, bookmarks and basic forms. It adapts
to DPI and screen safe areas and does not run JavaScript or a full CSS engine.

## Touch and controller games

These nine SDK 2.1 games fit a 288×224 logical canvas to the actual display, with touch/controller input, checked saves, background suspension and a top-right pause menu. Simplified Chinese is the default; the fourth menu row switches to English and saves the language separately from game progress. Builds support signed AOT packages for the simulator, ESP32-S3 and ESP32-S31:

| Application | Gameplay |
| --- | --- |
| [2048](game-2048/README.md) | Swipe to merge tiles, undo, saved board and high score |
| [Pocket Chess](pocket-chess/README.md) | Chess against the computer or another player, legal moves and promotion |
| [Pocket Sokoban](pocket-sokoban/README.md) | 34 box-pushing levels, undo, move/push counters and saved unlocks |
| [Pet Garden](pet-garden/README.md) | Original pixel pet, cozy home/garden, three evolution routes, decorations, berry catching and rhythm minigames |
| [Crisp Arcade](crisp-arcade/README.md) | Original MIT CAST N, GRAPPLING H, REFLECTOR and COLOR ROLL, with separate high scores |
| [Marble Islands](marble-islands/README.md) | Eight software-rendered 3D marble islands, gems, timed gates, pop bumpers, braking and medals |
| [Lantern Grove](lantern-grove/README.md) | Original CC0 idle pixel woodland, three resources, four building types, eight milestones and capped offline production |
| [Harbor Cafe](harbor-cafe/README.md) | Original CC0 seaside café, customer orders, drink preparation, table service, supplies, three upgrade routes and daily results |
| [Tiny Elements](tiny-elements/README.md) | Original CC0 falling-sand sandbox, water/fire/steam/plant/lava reactions, free painting and four challenges |

The six newer games use [Pocket Play](common/pocket-play/README.md); 2048, Chess and Sokoban use [Pocket Game](common/pocket-game/README.md). Keep the corresponding runtime and [Pocket UI](common/pocket-ui/README.md) when copying an app. All ten games keep action and event sounds with background music disabled. Lantern Grove, Harbor Cafe and Tiny Elements have a saved effects switch in their help pages.

[Anarch](anarch/README.md) has its own 160×120 logical canvas, fitted display and matching touch coordinates. Use the top-right menu control or controller X to switch languages, and the help control or Y to open instructions. Games inside the Game Boy and other emulators use the ROM's language. Each app documents controls, licensing, builds and reusable test entry points.

## Independent DevKit workflow

Install [DevKit](https://pxa.doit.am/docs/setup/), then from this repository's parent directory:

```sh
git clone https://github.com/SmartArduino/pxa-apps.git
pxa run pxa-apps/pixel-dungeon
pxa run pxa-apps/voxel-craft --profile sensecap-watcher
```

Close each simulator before the next command. All shipped display profiles support DPI and safe-area testing. Shared application sources stay under `common/`; keep the repository layout when copying examples. Firmware maintainers can use the workspace commands below.

Clone this repository to `local/pxa-apps` alongside `deps/pxa-system` in
`pxa-projects`. From the workspace root, for example:

```sh
tools/app.sh build store --target simulator --source-root local/pxa-apps
tools/simulator.sh product --profile pai-touch \
  --package local/app-output/pai-touch/pxa-store \
  --publisher-key deps/pxa-system/apps/pxa/.dev-signing/publisher-public.der
```

The store supports both a combined multi-target container and separate target packages. Use separate containers when target-specific assets differ:

```sh
for target in esp32s3 esp32s31 simulator; do
  tools/app.sh build store --target "$target" --aot-only \
    --output "local/app-output/store-variants/$target"
done
tools/app.sh build store --target wasm \
  --output local/app-output/store-variants/wasm
```

The AOT variants carry only their target architecture (plus any explicitly
WASM-only component); the portable variant contains only WASM components.

Workspace packaging defaults to the development-only key in
`deps/pxa-system/apps/pxa/.dev-signing`; supply `PXA_SIGNING_KEY` for your own
publisher. The bundled fixture is not a production credential. Local
`.dev-signing` files are ignored by this repo; never commit private signing
material.

Weather uses IP-based location and Open-Meteo without an API key; see [Weather](weather/README.md).

Application-specific source checks use the SDK and tools from the neighboring
`deps/pxa-system` checkout. Build outputs stay in the workspace's ignored
`local/app-output` directory.

Keep generated font previews and experimental source snapshots outside the
tracked source tree. Byte-identical display matrices and screenshots share one
attachment, with report links pointing to that retained file. Keep distinct
benchmark runs, test configurations, runtime resources and license notices.

Regression entry points (from the workspace root):

```sh
bash local/pxa-apps/tests/native/run.sh
bash local/pxa-apps/tests/native/reference/run.sh
CC=clang CXX=clang++ bash local/pxa-apps/common/cpp/tests/run.sh -DPXA_C_REFERENCE_BENCH=ON
bash local/pxa-apps/voxel-craft/tests/test.sh
python3 tools/test-app-matrix.py --output local/app-matrix --target all --sdk-examples
```

Multi-architecture builds share one locked Wasm build and then produce separate
AOT artifacts. `PXA_BUILD_CACHE_DIR` enables incremental builds;
`PXA_COMPILER_CACHE=ccache` optionally shares identical compiler work across
apps. Compiler caching adds no Guest or Host memory. Capacity definitions and
SDK/compiler inputs participate in compiler cache keys; do not reuse archives
built with different layout definitions by hand.

## Archived game ports

[68 independent games](common/retro-games/README.md) use SDK 2.1 with shared
aspect-fit rendering, input mapping and lifecycle handling. Puzzle, Arduboy and
SAF sources are shared; each game retains its own package ID and license.
See the [catalogue](common/retro-games/games.json). Existing Anarch, 2048,
Pocket Chess and Arcade's Minesweeper, Tetris and Brick Breaker are reused.

## SDL game ports

[L'Abbaye des Morts](abbaye-des-morts/README.md), [C-Dogs SDL](cdogs-sdl/README.md), [Project: Starfighter](starfighter/README.md) and [Neverputt](neverputt/README.md) are independent apps with icons and separate architecture packages. Their upstream code and asset licenses are preserved. A shared [SDL guest runtime](common/sdl-games/README.md) provides controller, touch, audio and bounded asset reads; SDL, the mixer and software GL run inside the app. C-Dogs currently has a 28 MiB linear memory budget, with additional Host memory required. Simulator validation does not establish hardware frame rates.

## Console emulators

Each platform is a separate App with isolated media and saves, using a shared PXA frontend:

[SNES](snes-emulator/README.md) · [Mega Drive](mega-drive-emulator/README.md) · [Master System](master-system-emulator/README.md) · [Game Gear](game-gear-emulator/README.md) · [PC Engine](pc-engine-emulator/README.md).
