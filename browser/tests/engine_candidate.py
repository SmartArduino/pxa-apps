#!/usr/bin/env python3
"""Compatibility checks for an isolated engine probe, never the logged-in app.

The probe takes a JS file, reports exception=0|1, then runs 1+2 in a fresh
realm. These process timings include Node startup and are NOT engine/FPS data.
An outer timeout also detects native RegExp work which ignores VM interrupts.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import tempfile


CASES = {
    "arithmetic": ("if (1+2!==3) throw Error('arithmetic');", "normal"),
    "modern": ("const f=(x=2)=>x+1; let a={x:2}; "
               "if(f()!==3 || a?.x!==2 || (null??4)!==4) throw Error('syntax');", "normal"),
    "descriptors": ("var x={}; Object.defineProperties(x,{value:{get:function(){return 7}}}); "
                    "if(x.value!==7 || !Object.hasOwn(x,'value')) throw Error('descriptor');", "normal"),
    "unicode_json": ("var x=JSON.parse('{\"title\":\"中文𠳐\"}'); "
                     "if(JSON.parse(JSON.stringify(x)).title!=='中文𠳐') throw Error('unicode');", "normal"),
    "regexp": ("if('abc123'.replace(/[0-9]+/g,'X')!=='abcX') throw Error('regexp');", "normal"),
    "promise": ("if(typeof Promise!=='function' || typeof Promise.resolve(1).then!=='function') "
                "throw Error('promise');", "normal"),
    "syntax_error": ("var = ;", "exception"),
    "infinite_loop": ("while(true) {}", "interrupted"),
    "out_of_memory": ("var a=[]; while(true) a.push('x'.repeat(1024));", "oom"),
    "regexp_deadline": ("/(a+)+$/.test('a'.repeat(60)+'!');", "bounded"),
}

RUNNER = """
import {WASI} from 'node:wasi';
import {readFile} from 'node:fs/promises';
const [, , wasm, directory, filename] = process.argv;
const wasi = new WASI({version:'preview1', args:['probe','/fixture/'+filename],
    env:{}, preopens:{'/fixture':directory}, returnOnExit:true});
const module = await WebAssembly.compile(await readFile(wasm));
const instance = await WebAssembly.instantiate(module,wasi.getImportObject());
process.exitCode = wasi.start(instance);
"""


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--wasm", required=True, type=Path)
    parser.add_argument("--node", default="node")
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--bridge", action="store_true", help="also parse/execute the app's full bridge source")
    parser.add_argument("--timeout", type=float, default=3)
    args = parser.parse_args()
    if not 0 < args.timeout <= 60:
        parser.error("--timeout must be in (0,60]")
    wasm = args.wasm.resolve(strict=True)
    cases = dict(CASES)
    if args.bridge:
        bridge = Path(__file__).resolve().parents[1] / "runtime/browser-runtime.js"
        source = ("var native={now:function(){return 0},cookie:function(){return ''},"
                  "storage:function(){return null},request:function(){return 1},cancel:function(){},"
                  "navigate:function(){},fragment:function(){},changed:function(){},wake:function(){},"
                  "url:'https://fixture.test/',userAgent:'PXA',width:296,height:240,density:1};\n"
                  "var bridge=" + bridge.read_text() + "(native);\n"
                  "bridge.open('p',['id','test'],false);bridge.text('before');bridge.close('p');bridge.finish();"
                  "document.getElementById('test').textContent='after';var html='';"
                  "for(var i=0;i<64;i++){var part=bridge.markup();html+=part.text;if(part.complete)break}"
                  "if(html.indexOf('after')<0)throw Error('markup');")
        cases["bridge_source"] = (source, "normal")
    results = []
    with tempfile.TemporaryDirectory(prefix="pxa-engine-probe-") as directory:
        root = Path(directory)
        runner = root / "run.mjs"
        runner.write_text(RUNNER)
        for name, (source, expected) in cases.items():
            filename = root / (name + ".js")
            filename.write_text(source)
            try:
                result = subprocess.run([args.node, "--no-warnings", str(runner), str(wasm), directory,
                                         filename.name], capture_output=True, text=True,
                                        timeout=args.timeout, check=False)
                exceptions = re.findall(r"^exception=([01])\b", result.stdout, re.MULTILINE)
                fatal = bool(re.search(r"^fatal=\d+ recovered$", result.stdout, re.MULTILINE))
                recovered = bool(len(exceptions) >= (1 if fatal else 2) and
                                 exceptions[-1] == "0" and result.returncode == 0)
                first = "exception" if fatal or (exceptions and exceptions[0] == "1") else "normal"
                if expected == "oom":
                    outcome = bool(re.search(r"^fatal=10 recovered$", result.stdout, re.MULTILINE) or
                                   "out of memory" in result.stdout.lower())
                elif expected == "interrupted":
                    outcome = "abort=1" in result.stdout or "interrupted" in result.stdout.lower()
                else:
                    outcome = expected == "bounded" or first == expected
                passed = recovered and bool(exceptions) and outcome
                results.append(dict(case=name, passed=passed, expected=expected, exit=result.returncode,
                                    fresh_realm_passed=recovered, fatal_recovered=fatal,
                                    stdout=result.stdout, stderr=result.stderr))
            except subprocess.TimeoutExpired:
                results.append(dict(case=name, passed=False, expected=expected, timed_out=True))
    report = dict(wasm_sha256=hashlib.sha256(wasm.read_bytes()).hexdigest(), wasm_bytes=wasm.stat().st_size,
                  scope="isolated WASI probe; not browser integration, native-stack, FPS or device validation",
                  promise_scope="constructor/then presence only; not microtask ordering",
                  all_passed=all(row["passed"] for row in results), results=results)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n")
    print(f"{sum(row['passed'] for row in results)}/{len(results)} passed; {args.output}")
    return 0 if report["all_passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
