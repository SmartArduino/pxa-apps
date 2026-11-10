#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Resize the maintained launcher portrait independently of animation updates."""
from pathlib import Path
from PIL import Image

app = Path(__file__).resolve().parents[1]
with Image.open(app / "art/launcher.png") as source:
    if source.width != source.height:
        raise ValueError("Launcher artwork must be square")
    icon = source.convert("RGB").resize((192, 192), Image.Resampling.NEAREST)
    icon.save(app / "assets/icon.png", optimize=True)
