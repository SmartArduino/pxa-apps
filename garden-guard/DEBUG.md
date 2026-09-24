# Garden Guard Debug Build

`GARDEN_DEBUG_UNLOCK_ALL` is disabled by default. Set it to `1` at package
compile time to unlock every level and plant for the current run:

```sh
PXA_APP_DEFINES=GARDEN_DEBUG_UNLOCK_ALL=1 \
  tools/package/package_app.sh garden-guard simulator \
  out/packages/pxa-garden-guard
```

The debug override is applied after loading storage, but is never written back
to `garden.save`. Rebuild without `PXA_APP_DEFINES` to return to normal saved
progress:

```sh
tools/package/package_app.sh garden-guard simulator \
  out/packages/pxa-garden-guard
```

Run these commands from the `pxa-system` repository root. The product
repository's `scripts/rebuild_pxa_simulator.sh --app garden-guard` wrapper
accepts the same comma-separated `PXA_APP_DEFINES` environment variable.
