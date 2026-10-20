#!/usr/bin/env python3
"""Compare the opt-in WASM read transfer with ordinary libc copies."""
import os
from pathlib import Path
import subprocess
import tempfile
here=Path(__file__).resolve().parent
sdk=here.parents[4]/'deps/pxa-system'
with tempfile.TemporaryDirectory(prefix='pxa-read-copy-') as name:
    out=Path(name)
    clang=os.environ.get('WASI_CLANG',str(sdk/'.pxa/wasi-sdk-34.0-x86_64-linux/bin/clang'))
    (out/'run.cjs').write_text('''const {WASI}=require('node:wasi');const fs=require('node:fs');
const wasi=new WASI({version:'preview1',args:[],env:{},preopens:{}});
WebAssembly.instantiate(fs.readFileSync(process.argv[2]),{wasi_snapshot_preview1:wasi.wasiImport}).then(({instance})=>wasi.start(instance));
''')
    for bulk in ('0','1'):
        subprocess.run([clang,'-std=c11','-O2','-fno-builtin','-DPXA_RECROF_BULK_READ_COPY='+bulk,
            '-I'+str(here.parent),str(here/'read_copy_test.c'),'-o',str(out/'test.wasm')],check=True)
        subprocess.run(['node',str(out/'run.cjs'),str(out/'test.wasm')],check=True)
