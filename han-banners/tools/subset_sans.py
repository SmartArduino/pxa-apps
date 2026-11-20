#!/usr/bin/env python3
"""Rebuild the renamed, OFL-licensed large-screen font subset."""

import hashlib
import json
import sys
from pathlib import Path

from fontTools import subset
from fontTools.ttLib import TTFont

root = Path(__file__).resolve().parents[1]
font_dir = root / "tools" / "fonts"
source = Path(sys.argv[1])
font = TTFont(source)
chars = set(map(chr, range(32, 127)))
for name in ("main.cpp", "expansion_ui.inc", "campaign.hpp", "world.hpp",
             "chapters.hpp", "battle.hpp", "save.hpp"):
    chars.update(c for c in (root / name).read_text() if ord(c) > 127)
missing = chars - {chr(n) for n in font.getBestCmap()}
if missing:
    raise ValueError("Missing glyphs: " + "".join(sorted(missing)))
options = subset.Options()
options.name_IDs = ["*"]
options.name_languages = ["*"]
subsetter = subset.Subsetter(options=options)
subsetter.populate(text="".join(sorted(chars)))
subsetter.subset(font)
names = {1: "Han Banners Sans", 2: "Medium", 3: "Han Banners Sans Medium 1.0",
         4: "Han Banners Sans Medium", 6: "HanBannersSans-Medium",
         16: "Han Banners Sans", 17: "Medium", 18: "Han Banners Sans Medium"}
for record in font["name"].names:
    if record.nameID in names:
        record.string = names[record.nameID].encode(record.getEncoding())
cff = font["CFF "].cff
cff.fontNames = ["HanBannersSans-Medium"]
cff[0].FamilyName = "Han Banners Sans"
cff[0].FullName = "Han Banners Sans Medium"
target = font_dir / "HanBannersSans-Medium.otf"
font.save(target)


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


license_file = root / "assets" / "Han-Banners-Sans-OFL.txt"
(font_dir / "sans.json").write_text(json.dumps({
    "upstream": "https://github.com/adobe-fonts/source-han-sans",
    "original_file": source.name,
    "original_sha256": digest(source),
    "font_file": target.name,
    "font_sha256": digest(target),
    "license_file": "../../assets/Han-Banners-Sans-OFL.txt",
    "license_sha256": digest(license_file),
}, indent=2) + "\n")
