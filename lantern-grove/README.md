# Lantern Grove

A small CC0 woodland for PXA SDK 2.1. Trees grow through five visible canopy stages, dew wells and keeper cabins become real objects, lanterns multiply, and a two minute morning/moonlight cycle brings fireflies over a stream. The painted background was generated for this app with built-in ImageGen; source and prompt are in assets/scene.png and assets/scene-source.json. Dynamic buildings, fireflies and original synthesized cues are drawn by the app. The app is `pxa-lantern-grove` version 1.4.0, release sequence 7, with 1 MiB pinned memory and a 64 KiB stack.

## Play

Three resources are shown below the title: **L**umen, **D**ew and **S**eed. Automatic income gathers in the basket, separate from spendable inventory. COLLECT brings all three home and adds one Lumen for tending, at most once every 0.65 foreground seconds. Start with 12 L / 8 D / 4 S and a level-one canopy. Base production is 1 L/s, 0.60 D/s and 0.28 S/s, so the first harvest and upgrades take seconds. The basket holds 120 of each resource initially. Full baskets discard excess income; spendable inventory is capped at 999999 per resource.

BUILD shows all current costs and the selected upgrade's exact benefit. Trees and buildings change the landscape immediately. Costs use the current level `n`:

| Upgrade | L / D / S cost | Each level adds | Maximum |
| --- | --- | --- | --- |
| Canopy | 8n² / 4n / (3n if n>2, else 0) | 1 L/s, 0.1 D/s, 0.08 S/s | 5 |
| Dew well | 10+12n / 0 / 3+3n | 0.65 D/s, 1500 basket capacity | 4 |
| Lantern | 18+80n² / 8+24n² / 4+10n² | 0.7 L/s, 0.06 S/s | 4 |
| Keeper cabin | 22+160n² / 12+80n² / 6+35n² | 0.3 L/s, 0.22 S/s, 3000 basket capacity | 3 |

JOURNAL contains eight sequential milestones: collect 10 total; canopy 2; well 1; collect 60 total; lantern 1; cabin 1; canopy 4 plus well 2; lantern 3 plus cabin 2 and collect 1800 total. Each milestone must be claimed, and rewards `5k L / 2k D / 2k S` for milestone number k. Completed earlier conditions remain satisfied. NEXT or LEFT/RIGHT changes journal chapter. The included native route reaches the festival in 202 simulated production seconds through actual costs, collecting every three seconds without altering inventories. The first two milestones remain reachable within 30 seconds. Maximum basket capacity is 15120 of each resource; well and cabin upgrades leave meaningful room for away-time harvests.

Touch controls fit completely above logical y=212. Use inside taps with x>=28 on the left. The runtime menu occupies logical x240–263/y3–20. Native system text stays sharp after scaling; the renderer respects display safe insets and rounded corners. Tap a visible firefly to collect the basket, with the same +1 L tending reward and 650 ms cooldown as COLLECT. The current goal and its progress appear above the scene; tap the goal to open JOURNAL. Dusk and dawn fade smoothly. The three lower buttons remain available. GUIDE explains the cycle, costs, time and controls, and toggles sound. BUILD, JOURNAL and GUIDE have a visible `< BACK` HUD button; tap logical (220,26) to return home while retaining all growth. Its touch region is x184–263/y20–42, with the visible frame above the resource strip. All pages and journal chapters can be reached and left using touch alone.

On a controller, arrows choose items, A collects/buys/claims, B returns home, X opens BUILD, Y opens JOURNAL, SELECT opens GUIDE and L/R cycles pages. Journal LEFT/RIGHT switches chapter. START opens the runtime menu. RESTART returns home while preserving every resource, building and milestone. Pointer cancellation or a release outside the original control never activates it. The default UI is Chinese; the common pause menu switches to English, including help, building names, milestones, and active notices. There is no background music. Selection, collection, building, milestone and error actions retain short original CC0 synthesized cues. GUIDE’s audio switch controls these effects and reuses the existing v1 sound field. Pause/background/exit silence is handled by the runtime; the game works with denied audio permission or sound off.

## Time and storage

Automatic production has one time source per process: synchronous standard WASI `clock_gettime(CLOCK_MONOTONIC)`. Each tick, action and serialization settles the difference from its last in-memory sample and advances that sample immediately. Long gaps are capped to eight hours and use constant work. Runtime pause/background freezes animation and the active tending cooldown, while automatic production is settled on the next sample, including on resume or an exit snapshot. It does not add `elapsed_ms` income on top. If the standard monotonic clock fails, only foreground elapsed production is available.

Across processes, `CLOCK_REALTIME` UTC seconds must lie in `[1704067200,4133980800)` (2024-01-01 through 2100-12-31 UTC). The save stores a UTC high-water mark. Load settles a positive difference once, capped to eight hours and basket capacity, then establishes the new monotonic sample. Backward UTC awards nothing and never lowers a valid saved mark. Invalid UTC explicitly disarms cross-process rewards; a later valid reading establishes a fresh anchor without retroactive income. Monotonic time still provides same-process background accumulation. A valid RTC on an ESP32 depends on the host having initialized real time; the app never fabricates a calendar or time from frame count. Both WASI clock features are declared; no new runtime event handler or hook is used.

The save key is `pxa-lantern-grove.save.v1`. The explicit 128-byte little-endian LGV1 version-one format contains resources, pending basket, fractional production, four levels, sequential milestone mask, sound/page selection, lifetime counters, day cycle, total production duration and UTC high-water. Bytes 80–123 and header reserved bytes must be zero; FNV-1a32 over bytes 0–123 occupies bytes 124–127. All fields, caps, ranges, timestamp, version, length and checksum are validated into a candidate before replacing state. Runtime persistence uses immutable transactional buffers, 200 ms coalescing and failure retries. Actions request a save; automatic saves run every 12 foreground seconds, with additional runtime snapshots on pause, background and exit. There are no per-tick storage writes.

## Build and test

From the workspace root, choose an ignored artifact directory and keep cache/tmp separate from other applications:

```sh
artifact_root="$PWD/local/artifacts/pocket-worlds/$(date +%Y%m%d-%H%M%S)/lantern-grove"
mkdir -p "$artifact_root/cache" "$artifact_root/tmp" "$artifact_root/native"
c++ -std=c++17 -Wall -Wextra -Werror -pedantic \
  -fsanitize=address,undefined -fno-omit-frame-pointer -g \
  local/pxa-apps/lantern-grove/core.cpp local/pxa-apps/lantern-grove/main.cpp \
  local/pxa-apps/lantern-grove/tests/runtime_stub.cpp \
  local/pxa-apps/lantern-grove/tests/native.cpp -o "$artifact_root/native/test"
"$artifact_root/native/test" > "$artifact_root/native/results.log" 2>&1
PXA_BUILD_CACHE_DIR="$artifact_root/cache" TMPDIR="$artifact_root/tmp" \
  tools/app.sh build lantern-grove --source-root local/pxa-apps \
  --target simulator,esp32s31,esp32s3 --aot-only --output "$artifact_root/package"
```

Native tests include a real progression route, debit/refusal/caps, sequential rewards, each save bit corrupted, recomputed-checksum malformed fields, UTC rollback/long/invalid/repeat cases, real hook pause→serialize→resume, background→exit→load, invalid→valid clocks, pointer cancel/outside release, reset retention and draw-only text bounds with sanitizer framebuffer guards. The production clock implementation is built by all three package targets.

The reusable integration entry is `tests/integration.py --simulator PATH --package SIGNED_DIR --publisher-key PATH --output ARTIFACT_DIR`. It uses actual simulator touch/controller input and independently reads checksummed host KV, without injecting saves. Icon generation uses Pillow: `python3 local/pxa-apps/lantern-grove/assets/icon.py`. Reports, logs, screenshots, packages and build caches belong only in ignored local artifacts.
