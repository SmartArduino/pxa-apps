#!/usr/bin/env python3
"""Export small original outline icons with the media apps' PNG size tiers."""
from pathlib import Path
import cairosvg

root=Path(__file__).resolve().parents[1]/'assets/ui'
root.mkdir(exist_ok=True)
icons={
 'mic': ('<rect x="9" y="2" width="6" height="12" rx="3"/><path d="M5 10v2a7 7 0 0 0 14 0v-2M12 19v3M8 22h8"/>', ['178577']),
 'history': ('<path d="M3 11a9 9 0 1 1 2.6 7.4M3 4v7h7M12 7v5l3 2"/>',['203b38']),
 'swap': ('<path d="M3 7h17M16 3l4 4-4 4M21 17H4M8 13l-4 4 4 4"/>',['178577']),
 'trash': ('<path d="M3 6h18M9 6V3h6v3M5 6l1 15h12l1-15M10 10v7M14 10v7"/>',['718580']),
 'stop': ('<rect x="5" y="5" width="14" height="14" rx="2" fill="currentColor" stroke="none"/>',['178577','ba5047']),
}
for name,(shapes,colors) in icons.items():
 for color in colors:
  svg=f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" color="#{color}" stroke="#{color}" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round">{shapes}</svg>'
  for size in [18,24,30,48]:
   cairosvg.svg2png(bytestring=svg.encode(),write_to=str(root/f'{name}-{color}-{size}.png'),output_width=size,output_height=size)
