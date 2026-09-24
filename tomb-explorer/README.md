# Tomb Explorer

Third-person room-and-portal explorer, ported from
`micropixel/guest/apps/tomb-explorer` to the PXA Guest SDK. Rooms are joined by
portals, a low-polygon explorer walks on sector floors with step-up, drop and
jump handling, and a follow camera orbits behind it.

The original drew with the micropixel `MeshRenderer` over Host
`TRIANGLE`/`QUAD` records. This port keeps the level data, collision, character
and camera code and submits to the PXA GameRender raster service instead:

| Original | PXA port |
|---|---|
| `sdk/mesh_renderer.hpp` front end | `mesh.c` |
| Host painter's ordering table | PXA `PAINTER_POLYGON`, same 512-bucket far-to-near ordering |
| Affine subdivision pass | same four-way recursive subdivision and affine scanlines |
| 16-row lit palette | `LIT_PALETTE_RGB565`, selected directly by interpolated light level |
| `HostSurface` + palette/texture upload | `pxa_game_render_create` + `pxa_raster_upload_*` |
| `world/level_data.cpp` | `level_data.c` (generated) |
| `gfx/textures.cpp` | `textures.c` + `textures.h` (generated) |

Portal visibility, room scissors, sector collision, lighting, touch controls and
the character poses are direct ports. Rooms are visited far to near, polygons
are clipped to their portal rectangle and then flushed through the ordering
table without a depth buffer. Polygons are dropped once the 2048-slot pool or
the 48 KiB / 768-command draw list is full.

## Layout

| Path | Role |
|---|---|
| `main.c` | app lifecycle, GameRender context, frame loop, stats, pointer node |
| `mesh.*` | polygon front end: transform, near clip, cull, light, scissor clip, records |
| `world.*` | portal traversal (visible rooms, draw order, scissors) and floor/ceiling queries |
| `player.*` | walking, steps, drops, jumps, wall sliding, room changes, camera |
| `character.*` | eleven-box explorer with procedural walk/jump poses |
| `input.*` | left-half stick, right-half orbit/tilt drag, tap to jump |
| `palette.*` | 16 light rows x 256 entries, including the original dark blue tint |
| `textures.*` | generated 64x64 INDEX8 textures |
| `level.h` | read-only level contract (rooms, sectors, portals, faces) |
| `level_data.c` | generated from `tools/level.json` |

## Regenerate assets

```sh
python3 tools/generate_textures.py   # textures.h, textures.c
python3 tools/generate_level.py      # level_data.c
python3 tools/generate_textures.py --check
python3 tools/generate_level.py --check
```

Both generators are ports of the micropixel originals with the same drawing
code, seeds and validation; only the emitted language differs.

## Build and run

```sh
tools/app.sh build tomb-explorer --board pai-touch --source-root deps/pxa-system/apps/pxa
tools/simulator.sh product --profile pai-touch \
    --package local/app-output/pai-touch/pxa-tomb-explorer \
    --publisher-key deps/pxa-system/apps/pxa/.dev-signing/publisher-public.der
```

Package defines:

| Define | Effect |
|---|---|
| `TOMB_RENDER_SCALE_SHIFT=N` | override automatic scaling and render at display >> N (`0..3`) |
| `TOMB_BENCHMARK=1` | scripted route through every room, fixed 30 Hz step, stats every 120 frames |
| `TOMB_PERF=1` | log the stats window while playing |

By default the GameRender context follows the actual dimensions reported by
the UI environment. Displays up to 800x480 render at native resolution; larger
even-sized displays render at half resolution, matching the original app's
automatic HostSurface policy.

The stats line reads
`fps_x100 rooms faces culled polygons subdivided overdraw_x100 dropped room`;
`dropped` must stay 0 or the draw list budget needs attention.

## Why this renderer

Tomb Explorer deliberately keeps the original painter-style renderer instead
of enabling a Z buffer and perspective-correct UV interpolation. Its closed
rooms, portal traversal and ordering table already provide a reliable
far-to-near draw order, while recursive subdivision keeps affine texture error
small enough for the intended PlayStation-era look.

This is the better trade-off on the ESP32-S31: it avoids clearing, reading and
writing an 800x480 depth buffer, avoids a reciprocal for perspective-correct
UVs at each pixel, and selects lighting from a precomputed 16-row RGB565
palette instead of multiplying colors per pixel. That leaves the CPU and
memory bandwidth for clipping and rasterization. It is an application-specific
choice rather than a general replacement for depth buffering; scenes with
freely intersecting geometry would still need a different visibility strategy.

## Controls

Left half of the panel is a virtual stick whose first touch fixes its centre.
Dragging on the right half orbits (horizontal) and tilts (vertical) the camera;
a short tap on the right half jumps. A connected controller's A button also
jumps.
