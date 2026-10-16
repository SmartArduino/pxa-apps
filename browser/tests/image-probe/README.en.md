# Offline image diagnostic

[中文](README.md) | English

This optional fixture measures the SDK/Host image path independently of Wi-Fi,
website scripts and the browser's UI. It adds no code or buffers to the browser.
The animation and controls target pai-touch at 296×240; they are diagnostics,
not a general screen-layout example.

Generate the two original JPEGs with Python and Pillow, then build normally:

```sh
python browser/tests/image-probe/generate.py
pxa build browser/tests/image-probe --target simulator --output /tmp/image-probe-sim
pxa build browser/tests/image-probe --target esp32s3 --output /tmp/image-probe-s3
```

Run `browser/tests/image_probe_smoke.py` with `--simulator`, `--package`,
`--publisher-key`, `--ime-package` and `--output` to verify common/large decoding,
release/reload, actual colored pixels and animation in the simulator.

Use the usual publisher signing options and package install/run commands for
your environment. The package ID is `pxa-image-probe`; it needs no network or
audio permission. The ordinary browser and its data do not need replacing.

`Preparing` copies a packaged `.bin` into the fixture's own private file, using
4064 B of scratch. Wait for `IMAGE PROBE READY` before opening a measurement
window. `Decode` passes ownership of that File to the existing image SDK; the
Host reads encoded data directly, without a proportional Guest input buffer.
`Release` drops the decoded image, and the next Canvas frame removes the last
display reference. `Large` prepares the second source before another decode.

- Common: 600×1200 baseline JPEG, with valid APP metadata; 229,193 B under
  Pillow used for this run. This stays below the old geometric buffer's
  256 KiB boundary, allowing comparison under the default 512 KiB temporary
  ceiling. Confirm hashes in the generated `assets/manifest.json`.
- Large: 1199×2533 baseline JPEG, 90,591 B in this run. It tests the new bounded
  JPEG backend beyond the old one-million-source-pixel limit; this is a
  capability test, not a same-workload speed comparison.
- Both decodes use max width 236, height 1024, 32768 output pixels and 512 KiB
  encoded input. Prepared image pixels stay on the Host.

The logged `elapsed_us` is wall time from SDK decode request to completion,
including file reading, Host processing, queuing and Guest scheduling. It is
**not isolated JPEG CPU time**. Image preparation, screenshots and new UI glyph
initialization must be reported separately. `MEMORY START`/`STOP` define each
run's local heap window; a later plain `MEMORY` can still show historical minima.

The Canvas animates at most every 33,333 µs, using a fixed command packet. Read
panel completion intervals with `PERF START display` / `PERF STOP` and take
`SCREENSHOT AFTER_PRESENT` captures to check pixel changes. Software captures
and completion counters still need physical-panel confirmation. Uninstall this
fixture when finished; the package manager may retain its empty lock sidecar.
