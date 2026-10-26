#!/usr/bin/env python3
"""Compare resident sky pixels with the actual MCU PAK converter, on native/WASM."""
import os
from pathlib import Path
import subprocess
import tempfile

here = Path(__file__).resolve().parent
apps = here.parents[2]
source = (apps / 'quake/tools/MCUPackConverter/sky.c').read_text()
reference = source[source.index('#define SKYSIZE'):source.index('void addSky')]
with tempfile.TemporaryDirectory(prefix='quake-sky-cache-') as name:
    out = Path(name)
    (out / 'converter_sky.h').write_text(reference)
    common = ['-std=c11', '-O2', '-g', '-Wall', '-Wextra', '-Werror',
              '-I' + str(out), '-I' + str(apps / 'quake/port'),
              str(here / 'quake_sky_cache_test.c')]
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
