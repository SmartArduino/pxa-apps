#!/usr/bin/env python3
"""Exercise the actual cache's keys, invalidation, bounded eviction and verifier."""
import os
from pathlib import Path
import subprocess
import tempfile

here = Path(__file__).resolve().parent
port = here.parents[2] / 'quake/port'
with tempfile.TemporaryDirectory(prefix='pxa-light-cache-') as name:
    out = Path(name)
    (out / 'quakedef.h').write_text('''#pragma once
#include <stdint.h>
typedef struct { short extents[2]; unsigned surfNodeIndex; } msurface_t;
typedef struct { const void *lightdata; } brush_t;
typedef struct { brush_t *brushModelData; } model_t;
typedef struct { struct { int ambientlight; } r_refdef;
    struct { model_t *worldmodel; } cl; uint32_t blocklights[400]; } globals_t;
typedef struct { msurface_t *surf; int lightadj[4], surfmip; } drawsurf_t;
extern globals_t *_g;
extern drawsurf_t r_drawsurf;
extern unsigned char nodeHadDlight[32];
extern int r_fullbright;
''')
    (out / 'r_local.h').write_text('#include "quakedef.h"\n')
    common = (here.parent / 'port.h').read_text()
    copy_helper = common[common.index('#ifndef PXA_RECROF_BULK_READ_COPY'):common.index('#ifdef __cplusplus')]
    (out / 'port.h').write_text('#include <stddef.h>\n#include <string.h>\n' + copy_helper +
        '\nvoid port_error(const char *);\nvoid port_log(const char *,...);\n')
    original = (here.parents[2] / 'quake/vendor/quake/r_surf.c').read_text()
    start = original.index('    uint16_t *d = (uint16_t*) _g->blocklights + 1;')
    stop = original.index('\n#endif', start)
    packing = original[start:stop].replace('_g->blocklights', 'lights').replace('_g->r_lightwidth', 'width')
    (out / 'quake_original_light_pack.h').write_text(
        'static void original_pack(uint32_t *lights, int width, int size) {\n' + packing + '\n}\n')
    cmd = [os.environ.get('CC', 'cc'), '-std=c11', '-O2', '-g', '-Wall', '-Wextra', '-Werror',
           '-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-fno-strict-aliasing',
           '-DPXA_QUAKE_LIGHTMAP_CACHE=1', '-DPXA_QUAKE_RENDER_PROBE=1',
           '-I'+str(out), '-I'+str(port), str(here/'quake_lightmap_cache_test.c'),
           str(port/'lightmap_cache.c'), '-o', str(out/'test')]
    subprocess.run(cmd, check=True)
    subprocess.run([str(out/'test')], check=True,
                   env=dict(os.environ, ASAN_OPTIONS='detect_leaks=1', UBSAN_OPTIONS='halt_on_error=1'))

    sdk = here.parents[4] / 'deps/pxa-system'
    clang = os.environ.get('WASI_CLANG', str(sdk/'.pxa/wasi-sdk-34.0-x86_64-linux/bin/clang'))
    subprocess.run([clang, '-std=c11', '-O2', '-fno-strict-aliasing',
        '-DPXA_QUAKE_LIGHTMAP_CACHE=1', '-DPXA_QUAKE_RENDER_PROBE=1', '-DPXA_RECROF_BULK_READ_COPY=1',
        '-I'+str(out), '-I'+str(port), str(here/'quake_lightmap_cache_test.c'),
        str(port/'lightmap_cache.c'), '-o', str(out/'test.wasm')], check=True)
    (out/'run.cjs').write_text("const {WASI}=require('node:wasi');const fs=require('node:fs');\n"
        "const wasi=new WASI({version:'preview1',args:['test'],env:{},preopens:{},returnOnExit:true});\n"
        "process.exitCode=wasi.start(new WebAssembly.Instance(new WebAssembly.Module(fs.readFileSync(process.argv[2])),"
        "{wasi_snapshot_preview1:wasi.wasiImport}));\n")
    subprocess.run(['node',str(out/'run.cjs'),str(out/'test.wasm')],check=True)
