# Voxel Craft file resources

The published GameRender build loads immutable Host resources through Assets
1.0 and GameRender's `asset-bindings` feature. It contains no block/font pixel
arrays, palette construction buffers or upload staging buffer. Small rendering
constants and font metrics remain in Guest code. The optional CPU fallback
still uses `block_textures.c`; dead-code elimination removes that generator
from the default GameRender-only artifact.

`tools/generate_resources.py` compiles the original procedural generator on the
build machine and exports raw INDEX8 pixels plus RGB565 palette JSON. It reuses
the checked-in output of `tools/generate_fonts.py`, so normal regeneration does
not depend on an installed font or Pillow. `resources.json` lets the package
compiler convert each item to PXR1 and include it in the signed resource index.
Only `assets/` outputs enter the package; source pixels in `resources/` do not
also enter AOT/Guest memory.

```sh
python3 local/pxa-apps/voxel-craft/tools/generate_resources.py
```

The high-quality context uses 45 block faces, 3 antialiased font atlases, and
one 16-row light palette. It needs 49 resident cache entries, with only one
pending request and at most one temporary Guest resource handle. The product
Host now defaults to 64 entries; byte budgets remain separately enforced.
Restricted-capability Hosts load 15 side textures, a cut-out font and the
appropriate palette. This preserves the existing capability-dependent pixels.

`voxel_assets_begin` prepares a fresh context asynchronously. Each completion
binds one ready object and closes the Guest handle immediately. A resource-free
loading frame is shown until the whole set is ready. The application does not
draw partial bindings. Rendering subsequently reads no files and uploads no
textures. Context recreation can reuse cached objects. The cache, current
bindings and in-flight frames each retain their own references.

`voxel_assets_suspend` cancels an in-flight request while keeping progress and
ready bindings, then resumes at that item. Cancellation can race a queued
success, so the event handler also closes late handles from old requests without
binding them into a replacement context. Context replacement and application
stop call `voxel_assets_cancel`. Terminal load failures use bounded retries.

Native loader regression (from the workspace root):

```sh
cc -std=c99 -Wall -Wextra -Werror -Wno-attributes \
  -fsanitize=address,undefined -fno-omit-frame-pointer -g \
  -Ideps/pxa-system/sdk/guest-c/include \
  local/pxa-apps/voxel-craft/tools/test_resource_loading.c \
  local/pxa-apps/voxel-craft/voxel_assets.c \
  -o build/resource-refactor/voxel-resource-loading-test
build/resource-refactor/voxel-resource-loading-test
```

Actual signed-AOT comparison (build both packages first):

```sh
cmake -S deps/pxa-system/simulator/desktop -B build/simulator/pai-touch
cmake --build build/simulator/pai-touch --target pxsys_voxel_resources_test -j4
python3 tools/compare-voxel-resources.py \
  build/resource-refactor/baseline/simulator/pxa-voxel-craft \
  build/resource-refactor/apps/pxa-voxel-craft \
  local/pxa-apps/.dev-signing/publisher-public.der
```

The comparison fixes Clock NOW only in the test runner, keeps real frame
scheduling, injects slow resource reads, and checks all 48 textures, 4096 palette
colors and the full 240×320 menu byte for byte. It writes source-artifact hashes,
raw pixels and separated memory counters. It does not measure gameplay FPS or
claim a complete subsystem RAM peak; those remain part of the wider resource
refactor's acceptance work.
