# PXA Apps

Application sources for the PXA workspace. The PXA runtime, Guest SDK, packaging
tools, and a minimal `hello` smoke-test app remain in `pxa-system`. All product
applications and their shared art live here.

Clone this repository to `local/pxa-apps` alongside `deps/pxa-system` in
`pxa-projects`. From the workspace root, for example:

```sh
tools/app.sh build store --target simulator --source-root local/pxa-apps
tools/simulator.sh product --profile pai-touch \
  --package local/app-output/pai-touch/pxa-store \
  --publisher-key deps/pxa-system/apps/pxa/.dev-signing/publisher-public.der
```

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
