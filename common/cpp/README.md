# Optional C++ game adapters

These adapters use the standalone PXA C++ SDK. They are included by the two
arcade applications; they add no default buffers to unrelated applications.

- `draw.hpp`: C++ Frame/Upload facade for the ported rendering rules; callers
  supply fixed draw and upload storage.
- `input.hpp`: one transparent Canvas for pointer/controller delivery. It
  contributes no full-screen pixel buffer.
- `audio.hpp`: Host-owned decoding/playback, eight prepared sounds, one in-flight
  load and a fixed failure bitmap. The app supplies the existing bounded
  coroutine pool through Context.

The C++ SDK supplies optional `game_upload.hpp`, `game_quality.hpp`,
`ui_geometry.hpp` and `ui_controller.hpp`; see
`deps/pxa-system/sdk/guest-cpp/docs/arcade-games.zh-CN.md` in the workspace.

From the workspace root:

```sh
PXA_CPP_GAME_TEST_BUILD="$PWD/local/cpp-games/native" bash local/pxa-apps/common/cpp/tests/run.sh -DPXA_C_REFERENCE_BENCH=ON
```

`SANITIZE=ON` enables ASan/UBSan. The optional reference tests use the C SDK
only as the baseline; neither production game depends on it. Release tests
keep assertions enabled. The native comparison executes actual Host validation
and raster code; it does not measure device display FPS.

`tests/simulator_matrix.py` drives signed AOT packages, pointer input, actual
menu states and cold save reloads at six display profiles. The game reports
document its isolated PXADB service and build commands.

`tests/reference/run_aot_probe.py` reuses the flags and libraries from a
configured **Unix Makefiles** product simulator build. Before running it,
build its dependency target:

```sh
cmake --build build/simulator/pai-touch --target pxsys_voxel_resources_test --parallel
```

The observer measures the actual AOT Guest tick, Host execution and next LVGL
CPU pass, excluding sleep and device presentation. Jump samples 60 ready-scene
frames after warmup; Pixel samples cold-start memory on its static title.
`--lifecycle` checks C++ clock, rendered-frame and music-instance behavior
through background/foreground transitions. Peak counters belong to one fresh
process, and shared resource/cache counters must not be double-counted.

`--pixel-play --native-build <native-build>` instead loads an identical normal
new-game save (seed `0x51ed270b`) into C/C++ Guests and taps Search twelve times
at fixed 240 ms intervals. It measures four seconds of presentation, records
clock-frame CPU and synchronous input CPU separately, then backgrounds the
game and validates its actual private-storage snapshot: twenty-four turns and
byte-identical saved game state in all six runs. The observer excludes panel
transfer and does not claim a separate Guest-only time measurement.

Add `--controller` to run the C++ controller integration check. The original
C app subscribes for controller events on root 1 but filters input to node 2;
it therefore cannot provide a fair controller benchmark. The C baseline is
left intact. The C++ check chooses its subscribed Canvas and verifies the same
saved state reached by pointer input.
