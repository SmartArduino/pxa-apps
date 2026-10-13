#!/usr/bin/env python3
"""Copy the C reference for a fixed-seed device benchmark; preserve originals."""
import argparse
import json
import shutil
from pathlib import Path

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("source", type=Path, help="pxa-apps source root")
parser.add_argument("output", type=Path, help="isolated source root to create")
args = parser.parse_args()
assert args.source.resolve() != args.output.resolve()
reference = args.source / "common/cpp/tests/reference/c/pixel-dungeon"
game = args.output / "pixel-dungeon-reference"
shutil.copytree(reference, game, dirs_exist_ok=True)
for source in game.glob("*.[ch]"):
    source.write_text(source.read_text().replace('"../../../../../', '"../common/'))
shutil.copytree(args.source / "pixel-dungeon/assets", game / "assets", dirs_exist_ok=True)
# Only an explicitly prepared, isolated reference has a C build manifest.
manifest = json.loads((args.source / "pixel-dungeon/package.json").read_text())
manifest.update(id="pxa-pixel-dungeon-reference", name="Pixel Dungeon timing reference")
manifest["build"] = {"system": "direct", "linear_memory": {"maximum_bytes": 1048576, "pinned": True}}
component = manifest["components"][0]
component.pop("cmake_target", None)
component.pop("wasi", None)
component["sources"] = ["main.c", "game.c", "dungeon.c", "render.c", "input.c", "layout.c",
                        "font.c", "assets.c", "compat.c", "audio.c", "strings.c", "font_data.c"]
(game / "package.json").write_text(json.dumps(manifest, indent=2) + "\n")
(args.output / "common").mkdir(parents=True, exist_ok=True)
shutil.copyfile(args.source / "common/pxa_canvas_pixels.h", args.output / "common/pxa_canvas_pixels.h")

def replace(filename, before, after):
    path = game / filename
    source = path.read_text()
    assert source.count(before) == 1, (filename, "reference changed; review seed hook")
    path.write_text(source.replace(before, after))

before = """        g_game.run_seed = pd_rng_mix((uint32_t)(timestamp_us >> 8),
                                     (uint32_t)timestamp_us);"""
replace("main.c", before, """#ifdef PD_BENCHMARK_SEED
        g_game.run_seed = (uint32_t)PD_BENCHMARK_SEED;
#else
""" + before + "\n#endif")
before = """                    pd_game_reset(game, pd_rng_mix(
                        (uint32_t)timestamp_us ^ game->run_seed,
                        (uint32_t)(timestamp_us >> 32) ^ game->turn));"""
replace("input.c", before, """#ifdef PD_BENCHMARK_SEED
                    pd_game_reset(game, (uint32_t)PD_BENCHMARK_SEED);
#else
""" + before + "\n#endif")
before = "            pd_game_reset(game, pd_rng_mix(game->run_seed, game->turn + 1u));"
replace("input.c", before, """#ifdef PD_BENCHMARK_SEED
            pd_game_reset(game, (uint32_t)PD_BENCHMARK_SEED);
#else
""" + before + "\n#endif")
print("Prepared isolated C reference; build with PD_BENCHMARK_SEED=1374496523")
