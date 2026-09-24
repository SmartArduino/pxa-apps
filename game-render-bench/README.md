# GameRender Bench

This device benchmark runs nine four-second micro-benchmarks and two six-second
game scenes, then leaves the final result list on screen:

- RGB565 full-screen clear baseline (`2D-CL`);
- 192 native-size INDEX8 sprites in one sprite-batch record (`2D-SN`);
- 192 variably scaled INDEX8 sprites in one sprite-batch record (`2D-SS`);
- 192 transparent, additive INDEX8 sprites (`2D-AD`);
- 96 animated RGB565 flat quads (`2D-QD`);
- 24 perspective-correct, depth-tested textured quads (`2D-TX`);
- a 2D battle with 12 actors, health bars, 48 projectiles, additive explosions,
  and 32 destructible cover blocks (`GM-2D`);
- 3 rotating, depth-tested cubes with back-face culling (`3D-03`);
- 6 rotating, depth-tested cubes with back-face culling (`3D-06`);
- 12 rotating, depth-tested cubes with back-face culling (`3D-12`);
- a 3D battle with moving actors, depth-tested terrain, projectiles, four
  destructible obstacles, and depth-tested debris (`GM-3D`).

The result list reports `V/R/MS`: visible FPS, rendered FPS, and average Host
raster time per rendered frame. Comparing visible and rendered FPS exposes the
display/presentation ceiling separately from raster throughput. Cyan marks 2D
groups and amber marks 3D groups. Benchmark frames contain only the workload
under test, without a live HUD. The app requests 16 ms clock ticks and relies
on GameRender's latest-wins mailbox when rendering or presentation cannot keep
up.

By default, scenes keep a 296x240 logical coordinate system but rasterize into
a 148x120 target. The Watcher presentation path scales that target by exactly
2x, cutting the color/depth working set and the maximum number of shaded pixels
to one quarter without complicating scene code. Build with
`PXA_APP_DEFINES=PXA_GAME_RENDER_SCALE_SHIFT=0` to collect native-resolution
comparison numbers, or select shifts 1 through 3 for other integer scales.

Each completed group is also written through the PXA log service. Device logs
include the signed app id, component id, level, ANSI level color, and the same
`V/R/MS` values shown on screen. Results with average raster time at or above
50 ms use warning level.
