#!/usr/bin/env python3
"""Run the real texture-loader state machine with bounded fake flash reads."""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile

here = Path(__file__).resolve().parent
apps = here.parents[2]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--source', type=Path, default=apps / 'quake/vendor/quake/d_surf.c')
args = parser.parse_args()
source = args.source.read_text()
start = source.index('uint8_t textureCacheBuffer[MAX_TEXTURE_SIZE];')
stop = source.index('#define TEST_PIX_SAVED_SAME_TEXTURE', start)
source = source[start:stop]
# Lighting has its own differential test; keep all texture-loader code intact.
start = source.index('#if PRELOAD_NEXT_LIGHT_MAP\nvoid loadAndBuildLighting')
stop = source.index('static DRAWSURF_SEC void pauseLoadingTexture', start)
source = source[:start] + 'static void loadAndBuildLighting(int i) { (void)i; }\n' + source[stop:]
with tempfile.TemporaryDirectory(prefix='quake-texture-cache-') as name:
    out = Path(name)
    (out / 'texture_loader.h').write_text(source)
    common = ['-std=c11', '-O2', '-g', '-Wall', '-Wextra',
              '-Wno-pointer-to-int-cast', '-Wno-unused-variable',
              '-I' + str(out), str(here / 'quake_texture_cache_test.c')]
    subprocess.run([os.environ.get('CC', 'cc'), *common,
                    '-fsanitize=address,undefined', '-fno-omit-frame-pointer',
                    '-o', str(out / 'test')], check=True)
    subprocess.run([str(out / 'test')], check=True,
                   env=dict(os.environ, ASAN_OPTIONS='detect_leaks=1', UBSAN_OPTIONS='halt_on_error=1'))
    sdk = here.parents[4] / 'deps/pxa-system'
    clang = os.environ.get('WASI_CLANG', str(sdk / '.pxa/wasi-sdk-34.0-x86_64-linux/bin/clang'))
    subprocess.run([clang, *common, '-o', str(out / 'test.wasm')], check=True)
    (out / 'run.cjs').write_text("const {WASI}=require('node:wasi');const fs=require('node:fs');\n"
        "const wasi=new WASI({version:'preview1',args:[],env:{},preopens:{}});\n"
        "WebAssembly.instantiate(fs.readFileSync(process.argv[2]),{wasi_snapshot_preview1:wasi.wasiImport})"
        ".then(({instance})=>wasi.start(instance));\n")
    subprocess.run(['node', str(out / 'run.cjs'), str(out / 'test.wasm')], check=True)
