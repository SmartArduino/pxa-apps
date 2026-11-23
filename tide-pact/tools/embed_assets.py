"""Emit C++ definitions from the preprocessed and versioned game assets."""
import argparse
from pathlib import Path


def main():
    cli = argparse.ArgumentParser(description=__doc__)
    cli.add_argument("output", type=Path)
    cli.add_argument("source", type=Path)
    args = cli.parse_args()
    with args.output.open("w") as out:
        out.write('#include "assets.hpp"\nnamespace tide::art {\n')
        for name in ("chart", "relief", "chart_materials", "chart_actions", "journal_icons", "ship_portraits", "helm_icons", "skirmish", "fishery", "enclave", "harbor", "title_compact", "storm", "portraits", "pilot_ships", "pilot_marks", "broadside", "glyphs", "glyphs_compact", "glyphs_native", "ui_wide",
                     "wordmark", "skin", "controls", "tabs", "goods", "specialty_icons", "navigation", "shore_troops", "boarding", "cadiz_duel",
                     "duel_fx", "discoveries", "officers", "voyage_icons", "driftwood",
                     "towns", "town_icons", "town_walk", "port_people", "port_ui", "dispatch_icons",
                     "weather_icons", "outskirts", "inland", "calicut", "levada", "marsh", "nature", "flags", "shore_tasks"):
            data = (args.source / (name + ".pxi")).read_bytes()
            out.write(f'const unsigned char {name}[] =\n')
            for start in range(0, len(data), 48):
                out.write('"' + "".join(f"\\x{b:02x}" for b in data[start:start+48]) + '"\n')
            out.write(";\n")
        out.write("}\n")


if __name__ == "__main__":
    main()
