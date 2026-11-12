# MakeCode Arcade for PXA

Experimental standalone cartridge player based on
[whmzsu/makecode-arcade-libretro](https://github.com/whmzsu/makecode-arcade-libretro).
The application uses the PXT bytecode VM, image primitives and mixer. No
RetroArch frontend or JavaScript engine is installed in the PXA system.

## Cartridges and controls

Export `.pxt64` from the MakeCode Arcade editor: About → Experiments →
Experimental Hardware, select the VM hardware target, then Download. PNG game
cards must first be converted on a computer using
[makecode-arcade-cartconv](https://github.com/whmzsu/makecode-arcade-cartconv).
This app does not compile TypeScript or decode PNG game cards on the device.

Import `.pxt64` through the game library, or place files in the application's
system-assigned media directory. Flash/private `roms/` also works. The directory
is displayed by the library. The first release accepts files up to 512 KiB and
the standard 160×120 game display. Unsupported images show a load error.

Use direction keys, A/B and Start (the game's menu button). The touchscreen
overlay and physical/USB gamepad share these controls. The overlay's MENU opens
the PXA pause menu. A second controller currently controls the same player.
Settings and high scores use PXA's private game save files, limited to 32 keys,
127 UTF-8 bytes per key and 1024 bytes per value. Instant snapshots are unavailable.

## Build

Run from the `pxa-projects` workspace:

```sh
tools/app.sh build makecode-arcade --source-root local/pxa-apps \
  --target esp32s3,esp32s31,esp32p4,simulator \
  --output local/artifacts/makecode-arcade/$(date +%Y%m%d-%H%M%S)
```

The signed app requires 4 MiB of guest linear memory, plus system, AOT code,
display and audio resources outside the guest. ESP boards need adequate PSRAM
and a compatible PXA firmware. Building an architecture does not establish its
game performance or that every board has sufficient free memory.

## Port maintenance

`upstream.json` pins the source revision. `vendor/pxtapp` remains unmodified.
`tools/prepare_runtime.py` creates a patched runtime in the CMake build directory
and checks replacement anchors. Review every patch when updating the dependency.
The port patches 32-bit NumberBoxes, uses a bounded GC arena, fixes Wasm vtable
callback signatures, and replaces pthread frame scheduling with synchronous VM
steps. Filesystem settings and diagnostics use application adapters.

Explicit JavaScript `throw`/`try` recovery is not supported by this version;
a thrown exception or VM panic terminates the guest. Games using unsupported
hardware/community shims may fail to load. The emulator is experimental, and
compatibility must be established per cartridge. Native desktop throughput and
PXA desktop display FPS must not be presented as ESP32 measurements.

Native smoke test, without PXA:

```sh
cmake -S local/pxa-apps/makecode-arcade -B "$out/native" -DARCADE_NATIVE_TEST=ON
cmake --build "$out/native" -j8
"$out/native/arcade_smoke" /path/to/game.pxt64 600
```

For the actual 32-bit ABI, configure a separate directory with
`-DCMAKE_C_FLAGS='-m32 -fno-pie' -DCMAKE_CXX_FLAGS='-m32 -fno-pie'`
and `-DCMAKE_EXE_LINKER_FLAGS='-m32 -no-pie'` on a multilib Linux host.
The test checks repeated loading, deterministic video/audio and save validation.

`tests/simulator.py` runs the signed package with the real product simulator,
loads an externally supplied test cartridge and injects touch/gamepad input.
Pass `--simulator`, `--package`, `--publisher-key`, `--cartridge` and `--output`.
All test output directories must be inside the workspace's ignored
`local/artifacts/<topic>/<timestamp>/` tree. Test games are not redistributed in
the application package.

## Licenses

The PXT runtime and upstream core are MIT licensed; see `vendor/LICENSE` and
`vendor/LICENSE-MICROSOFT.txt`. The shared PXA emulator frontend and adapter are
GPL-2.0-or-later, with some shared display helpers under MPL-2.0 and UI fonts
under OFL-1.1. Notices are included in `assets/THIRD_PARTY_LICENSES.txt`. These
components belong to this independent application package; they are not linked
into the PXA system, firmware or Host services. Corresponding source includes
the shared `common/emulator`, `common/rom`, `common/controls` dependencies and
their original license notices.
