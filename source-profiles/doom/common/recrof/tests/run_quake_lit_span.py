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
    subprocess.run([str(out/"sanitized")],check=True,env=dict(os.environ,ASAN_OPTIONS="detect_leaks=1"))
    subprocess.run([*flags,"-O3",source,"-o",str(out/"bench")],check=True)
    subprocess.run([str(out/"bench"),"benchmark"],check=True)
