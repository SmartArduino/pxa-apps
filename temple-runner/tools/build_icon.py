#!/usr/bin/env python3
"""Rasterize the original vector package icon. Requires CairoSVG."""
from pathlib import Path
import cairosvg
root=Path(__file__).resolve().parents[1]
cairosvg.svg2png(url=str(root/'assets/icon.svg'),write_to=str(root/'assets/icon.png'))
