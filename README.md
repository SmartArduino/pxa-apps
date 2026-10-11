# PXA Apps

Application sources for the PXA workspace. The PXA runtime, Guest SDK, packaging
tools, and a minimal `hello` smoke-test app remain in `pxa-system`. All product
applications and their shared art live here.

All maintained applications now build as **C++26** against **Core 2.0**.
The production C versions of Jump Jump, Pixel Dungeon and Voxel Craft have
been retired; use [`jump-jump-3d-cpp`](jump-jump-3d-cpp/README.md),
[`pixel-dungeon-cpp`](pixel-dungeon-cpp/README.md) and `voxel-craft-cpp`.
Their frozen C algorithms remain only under `common/cpp/tests/reference/c`
for timing, pixel and gameplay comparisons; they have no package manifests.

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

Clone this repository to `local/pxa-apps` alongside `deps/pxa-system` in
`pxa-projects`. From the workspace root, for example:

```sh
tools/app.sh build store --target simulator --source-root local/pxa-apps
tools/simulator.sh product --profile pai-touch \
  --package local/app-output/pai-touch/pxa-store \
  --publisher-key deps/pxa-system/apps/pxa/.dev-signing/publisher-public.der
```

For store publication, build separate signed containers for each runtime
profile rather than a combined multi-AOT container:

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

The Weather example requires a local `weather/weather_config.h` defining
`PXA_WEATHER_API_KEY` as a quoted string. That file is ignored so API
credentials do not enter this repository.

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
bash local/pxa-apps/voxel-craft-cpp/tests/test.sh
python3 tools/test-app-matrix.py --output local/app-matrix --target all --sdk-examples
```

Multi-architecture builds share one locked Wasm build and then produce separate
AOT artifacts. `PXA_BUILD_CACHE_DIR` enables incremental builds;
`PXA_COMPILER_CACHE=ccache` optionally shares identical compiler work across
apps. Compiler caching adds no Guest or Host memory. Capacity definitions and
SDK/compiler inputs participate in compiler cache keys; do not reuse archives
built with different layout definitions by hand.
