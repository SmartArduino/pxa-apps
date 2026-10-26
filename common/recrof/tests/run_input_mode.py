#!/usr/bin/env python3
"""Exercise actual runtime drawing/input in both execution models and genres."""
import argparse
import json
import os
from pathlib import Path
import re
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--project-root", type=Path, required=True)
parser.add_argument("--output", type=Path, required=True)
parser.add_argument("--density-q16", type=int, default=65536)
args = parser.parse_args()
project = args.project_root.resolve()
out = args.output.resolve()
tests = Path(__file__).resolve().parent
out.mkdir(parents=True, exist_ok=True)
captures = [.1, .3, .5, .7, .9, 2.15, 2.8, 3.1, 3.3, 3.7, 3.9, 4.3, 4.7, 4.9]
visible = [True, True, True, True, False, False, True, True,
           False, True, True, False, False, True]
actions = "0.2:16:1:1,0.4:0,0.8:16,1.0:0,2.2:0,3.2:16,3.6:16,3.8:0,4.2:32,4.4:0,4.6:0:0:1,4.8:0:0"
reports = {}
for native in (False, True):
    for rts in (False, True):
        name = ("native" if native else "callback") + ("-rts" if rts else "-fps")
        source = out / "profiles" / name
        source.mkdir(parents=True, exist_ok=True)
        manifest = json.loads((tests / "fixture/package.json").read_text())
        for key in ("min_sdk", "target_sdk", "compile_sdk"):
            manifest[key] = [2, 1]
        if native:
            manifest["components"][0]["execution_model"] = "main-loop"
        (source / "package.json").write_text(json.dumps(manifest, indent=2) + "\n")
        (source / "CMakeLists.txt").write_text(
            'cmake_minimum_required(VERSION 3.20)\nproject(input_fixture C CXX)\n'
            f'set(PXA_RECROF_MAIN_LOOP {"ON" if native else "OFF"})\n'
            f'include("{tests.parent}/Engine.cmake")\n'
            f'pxa_recrof_component({"RTS " if rts else ""}TITLE "Input fixture" '
            f'SOURCES "{tests}/input_fixture.c"'
            f'{" DEFINITIONS PORT_ENGINE_CONTROLLER=1" if native and not rts else ""})\n')
        env = dict(os.environ, PXA_BUILD_CACHE_DIR=str(out / "cache"))
        with (out / f"{name}-build.txt").open("w") as log:
            subprocess.run(["bash", str(project / "tools/app.sh"), "build", name,
                            "--board", "doit-gamekit", "--target", "wasm",
                            "--source-root", str(source.parent),
                            "--output", str(out / "packages")],
                           cwd=project, env=env, stdout=log, stderr=subprocess.STDOUT, check=True)
        wasm, = (out / "cache").glob(f"{name}-wasm-*/artifacts/main.wasm")
        x, y = (300, 12) if rts else (292, 212)
        # PXA_POINTER_DOWN/MOVE/UP = 0/1/2.
        pointers = [[.6, 0, x, y], [1.2, 0, x, y], [1.3, 2, x, y],
                    [2.5, 0, x, y], [2.6, 1, x-10, y], [2.7, 2, x-10, y],
                    [3.0, 0, x, y], [3.4, 0, x, y], [4.0, 2, x, y]]
        env.update(PXA_TEST_DENSITY_Q16=str(args.density_q16),
                   PXA_TEST_MAIN_LOOP=str(int(native)), PXA_TEST_REQUIRE_TICK_START="1",
                   PXA_TEST_BACKGROUND="1", PXA_TEST_POINTERS=json.dumps(pointers),
                   PXA_TEST_CAPTURE_TIMES=json.dumps(captures))
        # Keep RTS A/B clicks; FPS uses shoulder fire/jump while A/B look.
        input_actions = actions if rts else actions.replace(':16', ':2048').replace(':32', ':1024')
        prefix = out / name
        with (out / f"{name}.log").open("w") as log:
            subprocess.run(["node", str(tests / "host.mjs"), str(wasm),
                            str(out / "assets"), str(out / f"{name}-private"),
                            str(prefix), "5.2", input_actions], env=env,
                           stdout=log, stderr=subprocess.STDOUT, check=True)
        report = json.loads(prefix.with_suffix(".json").read_text())
        if native and not rts:
            # One foreground tick launches the engine. Input, timed sleeps,
            # audio/lifecycle replies and resume must work without more ticks.
            assert 1 <= report["ticks"] <= 3, report["ticks"]
        else:
            assert report["ticks"] > 20, report["ticks"]
        assert len(report["captures"]) == len(captures)
        for shot, want_visible in zip(report["captures"], visible):
            assert abs(shot["seconds"] - shot["requested"]) < .15, shot
            pixels = Path(shot["file"]).read_bytes()
            border = 280 if rts else 192*320+268
            assert int.from_bytes(pixels[border*2:border*2+2], "little") == (
                0xffff if want_visible else 0x001f), shot
            if not want_visible:
                assert pixels == b"\x1f\x00"*(320*240), shot
        keys = [tuple(map(int, m.groups())) for line in report["logs"]
                if (m := re.fullmatch(r"INPUT FIXTURE key=(\d+) down=(\d+)", line))]
        expected = ([(202, 1), (201, 1), (202, 0), (201, 0)]*2 +
                    [(203, 1), (203, 0)]) if rts else [
                        (200, 1), (200, 0), (200, 1), (200, 0), (32, 1), (32, 0)]
        assert keys == expected, (name, keys, expected)
        assert not any("INPUT FIXTURE motion=" in line for line in report["logs"])
        assert "BACKGROUND RESUME PASS" in report["logs"]
        reports[name] = {"passed": True, "captures": len(captures), "keys": keys,
                         "scope": report["scope"]}
        print(name, "PASS", flush=True)
(out / "result.json").write_text(json.dumps(reports, indent=2) + "\n")
