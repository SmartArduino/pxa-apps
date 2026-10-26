#!/usr/bin/env python3
"""Compare all 8 cached kernels with the current vendored implementation."""
from pathlib import Path
import os, subprocess, tempfile
here=Path(__file__).resolve().parent
quake=here.parents[2]/"quake"
s=(quake/"vendor/quake/d_scan.c").read_text()
a=s.index("#define defineDrawLitSpan(miplevel, power2)")
b=s.index("\ndefineDrawLitSpan(0, 1)",a)
with tempfile.TemporaryDirectory(prefix="pxa-quake-lit-") as d:
    out=Path(d)
    (out/"quake_original_lit_span.h").write_text(s[a:b]+"\n")
    flags=[os.environ.get("CC","cc"),"-std=c11","-I"+str(out),"-I"+str(quake/"port")]
    source=str(here/"quake_lit_span_test.c")
    subprocess.run([*flags,"-O2","-g","-fsanitize=address,undefined","-fno-omit-frame-pointer",source,"-o",str(out/"sanitized")],check=True)
    subprocess.run([str(out/"sanitized")],check=True,env=dict(os.environ,ASAN_OPTIONS="detect_leaks=1", UBSAN_OPTIONS="halt_on_error=1"))
    subprocess.run([*flags,"-O3",source,"-o",str(out/"bench")],check=True)
    subprocess.run([str(out/"bench"),"benchmark"],check=True)
    sdk=here.parents[4]/"deps/pxa-system"
    clang=os.environ.get("WASI_CLANG",str(sdk/".pxa/wasi-sdk-34.0-x86_64-linux/bin/clang"))
    subprocess.run([clang,"-std=c11","-O3","-I"+str(out),"-I"+str(quake/"port"),
                    source,"-o",str(out/"test.wasm")],check=True)
    (out/"run.cjs").write_text("const {WASI}=require('node:wasi');const fs=require('node:fs');\n"
        "const wasi=new WASI({version:'preview1',args:['test'],env:{},preopens:{},returnOnExit:true});\n"
        "const moduleBytes=fs.readFileSync(process.argv[2]);\n"
        "process.exitCode=wasi.start(new WebAssembly.Instance(new WebAssembly.Module(moduleBytes),"
        "{wasi_snapshot_preview1:wasi.wasiImport}));\n")
    result=subprocess.run(["node",str(out/"run.cjs"),str(out/"test.wasm")],check=True,
                          text=True,capture_output=True)
    print(result.stdout,end="")
    if 'PASS: 200000 exact pixel comparisons' not in result.stdout:
        raise RuntimeError('WASM did not complete the pixel comparisons: '+result.stderr)
