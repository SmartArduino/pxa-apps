"""Regenerate bundled PNG icons: python3 generate_icons.py (requires rsvg-convert)."""

from pathlib import Path
import subprocess


ASSETS = Path(__file__).resolve().parent / "assets"
SUN = '<circle cx="32" cy="32" r="11" fill="#ffd04a"/><path d="M32 4v9m0 38v9M4 32h9m38 0h9M12 12l7 7m26 26 7 7M52 12l-7 7M19 45l-7 7" stroke="#ffbb36" stroke-width="4" stroke-linecap="round"/>'
CLOUD = '<path d="M15 46h34a10 10 0 0 0 0-20 17 17 0 0 0-32-4A12 12 0 0 0 15 46Z" fill="#e8f4fc" stroke="#91b8d1" stroke-width="3" stroke-linejoin="round"/>'
MOON = '<path d="M43 9a24 24 0 1 0 12 39A22 22 0 0 1 43 9Z" fill="#fce4a4" stroke="#efbd68" stroke-width="2"/>'
RAIN = '<path d="M22 48l-4 8m17-8-4 8m17-8-4 8" stroke="#39a5e5" stroke-width="4" stroke-linecap="round"/>'
SNOW = '<path d="M24 48v10m-5-5h10m13-5v10m-5-5h10" stroke="#63b7e8" stroke-width="3" stroke-linecap="round"/>'

ICONS = {
    "sun": SUN,
    "moon": MOON,
    "cloud": CLOUD,
    "partly-cloudy": '<g transform="translate(-10 -11) scale(.83)">' + SUN + '</g>' + CLOUD,
    "rain": CLOUD + RAIN,
    "snow": CLOUD + SNOW,
    "storm": CLOUD + '<path d="M34 42l-9 12h9l-4 9 18-18H37l5-7" fill="#ffcc36"/>',
    "fog": CLOUD + '<path d="M10 51h44M15 58h34" stroke="#8faec0" stroke-width="3" stroke-linecap="round"/>',
    "location": '<path d="M32 57S14 37 14 26a18 18 0 0 1 36 0c0 11-18 31-18 31Z" fill="#31a8dc"/><circle cx="32" cy="26" r="7" fill="white"/>',
    "clock": '<circle cx="32" cy="32" r="23" fill="#cfe7f6" stroke="#4e92ba" stroke-width="4"/><path d="M32 17v16l10 7" fill="none" stroke="#397da5" stroke-width="4" stroke-linecap="round"/>',
    "info": '<circle cx="32" cy="32" r="23" fill="#58a8d5"/><circle cx="32" cy="19" r="3" fill="white"/><path d="M32 29v16" stroke="white" stroke-width="5" stroke-linecap="round"/>',
    "refresh": '<path d="M51 26a20 20 0 1 0 0 14M51 13v13H38" fill="none" stroke="white" stroke-width="6" stroke-linecap="round" stroke-linejoin="round"/>',
}

for name, drawing in ICONS.items():
    svg = f'<svg xmlns="http://www.w3.org/2000/svg" width="64" height="64" viewBox="0 0 64 64">{drawing}</svg>'
    with (ASSETS / f"{name}.png").open("wb") as image:
        subprocess.run(["rsvg-convert", "-w", "64", "-h", "64"],
                       input=svg.encode(), stdout=image, check=True)
