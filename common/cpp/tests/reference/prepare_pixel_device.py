#!/usr/bin/env python3
"""Copy the C reference for a fixed-seed device benchmark; preserve originals."""
import argparse
import shutil
from pathlib import Path

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("source", type=Path, help="pxa-apps source root")
parser.add_argument("output", type=Path, help="isolated source root to create")
args = parser.parse_args()
assert args.source.resolve() != args.output.resolve()
shutil.copytree(args.source / "pixel-dungeon", args.output / "pixel-dungeon", dirs_exist_ok=True)
(args.output / "common").mkdir(parents=True, exist_ok=True)
shutil.copyfile(args.source / "common/pxa_canvas_pixels.h", args.output / "common/pxa_canvas_pixels.h")

def replace(filename, before, after):
    path = args.output / "pixel-dungeon" / filename
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
