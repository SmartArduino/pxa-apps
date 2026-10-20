#!/usr/bin/env python3
"""Exercise the actual queued Quake adapter and its atomic rejection fallback."""
import os
from pathlib import Path
import subprocess
import tempfile
from run_quake_projected_spans import write_reference, here, sdk
with tempfile.TemporaryDirectory(prefix='pxa-quake-frame-adapter-') as name:
    out=Path(name)
    write_reference(out)
    clang=os.environ.get('WASI_CLANG',str(sdk.parent/'.pxa/wasi-sdk-34.0-x86_64-linux/bin/clang'))
    probe_args=['-DPXA_QUAKE_RENDER_PROBE=1'] if os.environ.get('PXA_QUAKE_TEST_RENDER_PROBE')=='1' else []
    subprocess.run([clang,*probe_args,'-DPXA_FRAME_BULK_COPY='+os.environ.get('PXA_QUAKE_TEST_BULK_COPY','1'),'-DPXA_GAME_RENDER_WASM_PACKED_CODEC='+os.environ.get('PXA_QUAKE_TEST_PACKED_CODEC','0'),'-std=gnu11','-O2','-fno-builtin','-fwrapv','-ffp-contract=off',
        '-Wno-unused-variable','-Wno-incompatible-pointer-types-discards-qualifiers',
        '-I'+str(out),'-I'+str(sdk.parent/'sdk/guest-c/include'),
        '-I'+str(here.parents[2]/'quake/port'),'-I'+str(here.parent),
        str(here/'quake_frame_adapter_test.c'),'-Wl,-z,stack-size=131072','-o',str(out/'test.wasm')],check=True)
    (out/'run.cjs').write_text('''const {WASI}=require('node:wasi');const fs=require('node:fs');
const wasi=new WASI({version:'preview1',args:[],env:{},preopens:{}});
WebAssembly.instantiate(fs.readFileSync(process.argv[2]),{wasi_snapshot_preview1:wasi.wasiImport}).then(({instance})=>wasi.start(instance));
''')
    subprocess.run(['node',str(out/'run.cjs'),str(out/'test.wasm')],check=True)
