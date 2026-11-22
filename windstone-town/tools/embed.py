"""Embed production pixel assets so the game needs no resource download."""
from pathlib import Path
import sys

from palette import grade_atlas

output, assets = map(Path, sys.argv[1:])
with output.open("w") as cpp:
    cpp.write("namespace wind::art {\n")
    names = ("title", "sprites", "font", "terrain", "plants", "actors", "monsters",
             "ui", "icons", "effects", "relics", "caves", "walls", "stairs", "backdrop", "tools", "markers",
             "waterfall", "glints", "expedition", "folio", "walking", "combat", "items",
             "combat_body", "held_weapons", "worn_gear")
    for name in names:
        data = grade_atlas(name, (assets / (name + ".pxi")).read_bytes())
        cpp.write(f"extern const unsigned char {name}[] = {{\n")
        for offset in range(0, len(data), 24):
            cpp.write("    " + ",".join(str(b) for b in data[offset:offset+24]) + ",\n")
        cpp.write("};\n")
    cpp.write("}\n")
