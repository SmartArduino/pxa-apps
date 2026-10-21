#!/usr/bin/env python3
"""Compare native projection, light and depth to the current upstream functions."""
import os
from pathlib import Path
import re
import subprocess
import tempfile

here = Path(__file__).resolve().parent
sdk = here.parents[4] / 'deps/pxa-system/libpxa'
quake = here.parents[2] / 'quake/vendor/quake'
def write_reference(out):
    source = (quake / 'd_scan.c').read_text()
    a = source.index('#define defineDrawLitSpan(miplevel, power2)')
    b = source.index('\ndefineDrawLitSpan(0, 1)', a)
    reference = source[a:b].replace('__attribute__((noinline))',
        '__attribute__((noinline, no_sanitize("undefined")))')
    a = source.index('#if ESP_PLATFORM\n// parity: 0 even rows, 1 odd rows, -1 all (see D_DrawSpans8 below)')
    b = source.index('\n#if ESP_PLATFORM\n// The rows of a surface are split', a)
    reference += '\n' + source[a:b].replace('static void D_DrawSpans8Rows',
        '__attribute__((no_sanitize("undefined"))) static void D_DrawSpans8Rows')
    a = source.index('#if ESP_PLATFORM\n// parity: 0 even rows, 1 odd rows, -1 all (see D_DrawSpans8AndZ)')
    b = source.index('\n#if ESP_PLATFORM\nvoid D_DrawZSpans', a)
    depth = source[a:b].replace('static void D_DrawZSpansRows',
        '__attribute__((no_sanitize("undefined"))) static void D_DrawZSpansRows')
    (out / 'quake_original_lit_span.h').write_text(reference[:reference.index('\n#if ESP_PLATFORM\n// parity:')] + '\n')
    (out / 'quake_original_projected.h').write_text(reference[reference.index('\n#if ESP_PLATFORM\n// parity:'):] + '\n' + depth)
    table = (quake / 'tabmath.c').read_text()
    match = re.search(r'const int fixedReciprocal256\[256\]\s*=\s*(\{.*?\});', table, re.S)
    assert match
    (out / 'quake_reciprocal.h').write_text('static const int fixedReciprocal256[256] = ' + match[1] + ';\n')

def main():
    with tempfile.TemporaryDirectory(prefix='pxa-projected-spans-') as name:
        out = Path(name)
        write_reference(out)
        command = [os.environ.get('CC', 'cc'), '-std=c11', '-O2', '-g', '-Wall', '-Wextra',
            '-Wno-unused-variable', '-fwrapv', '-ffp-contract=off',
            '-fsanitize=address,undefined', '-fno-omit-frame-pointer',
            '-I' + str(out), '-I' + str(sdk / 'include'),
            str(here / 'quake_projected_spans_test.c'),
            str(sdk / 'src/services/game_render/raster_spans.c'),
            str(sdk / 'src/services/game_render/raster_frame_spans.c'),
            str(sdk / 'src/services/game_render/raster_blit.c'),
            str(sdk / 'src/services/game_render/raster_projected_spans.c'), '-o', str(out / 'test')]
        command.insert(command.index('-o'), str(sdk / 'src/services/game_render/raster_prepared.c'))
        subprocess.run(command, check=True)
        subprocess.run([str(out / 'test')], check=True,
            env=dict(os.environ, UBSAN_OPTIONS='halt_on_error=1', ASAN_OPTIONS='detect_leaks=1'))

if __name__ == '__main__':
    main()
