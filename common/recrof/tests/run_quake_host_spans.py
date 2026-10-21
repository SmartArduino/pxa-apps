#!/usr/bin/env python3
"""Differential-test GameRender against the current vendored Quake kernel."""
import os
from pathlib import Path
import subprocess
import tempfile

here = Path(__file__).resolve().parent
root = here.parents[4]
sdk = root / 'deps/pxa-system/libpxa'
quake = here.parents[2] / 'quake'
source = (quake / 'vendor/quake/d_scan.c').read_text()
start = source.index('#define defineDrawLitSpan(miplevel, power2)')
end = source.index('\ndefineDrawLitSpan(0, 1)', start)
with tempfile.TemporaryDirectory(prefix='pxa-host-spans-') as name:
    out = Path(name)
    # Upstream deliberately left-shifts negative fixed-point coordinates.
    # Preserve its exact reference arithmetic; sanitize the new Host kernel.
    reference = source[start:end].replace('__attribute__((noinline))',
        '__attribute__((noinline, no_sanitize("undefined")))')
    (out / 'quake_original_lit_span.h').write_text(reference + '\n')
    cmd = [os.environ.get('CC', 'cc'), '-std=c11', '-O2', '-g', '-Wall', '-Wextra',
           '-fsanitize=address,undefined', '-fno-omit-frame-pointer',
           '-I' + str(out), '-I' + str(sdk / 'include'),
           str(here / 'quake_host_spans_test.c'),
           str(sdk / 'src/services/game_render/raster_spans.c'),
        str(sdk / 'src/services/game_render/raster_frame_spans.c'),
        str(sdk / 'src/services/game_render/raster_blit.c'),
           str(sdk / 'src/services/game_render/raster_projected_spans.c'), '-ffp-contract=off', '-o', str(out / 'test')]
    cmd.insert(cmd.index('-o'), str(sdk / 'src/services/game_render/raster_prepared.c'))
    subprocess.run(cmd, check=True)
    subprocess.run([str(out / 'test')], check=True,
        env=dict(os.environ, UBSAN_OPTIONS='halt_on_error=1', ASAN_OPTIONS='detect_leaks=1'))
