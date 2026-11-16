#!/usr/bin/env python3
"""Prepare device-sized logo/icon PNGs from the project's generated originals.
Only size conversion is performed; semantic artwork changes use imagegen.
Requires ImageMagick 7 (magick).
"""
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[1]
for source,destination,size in (
 ('resources/brand/app-icon-source.png','assets/icon.png','128x128'),
 ('resources/brand/title-logo-source.png','resources/ui/title-logo.png','432x144')):
 target=root/destination;target.parent.mkdir(parents=True,exist_ok=True)
 subprocess.run(['magick',str(root/source),'-resize',size,'-background','none','-gravity','center','-extent',size,'-strip',str(target)],check=True)
 print(target)
 if destination=='assets/icon.png':
  # The device launcher accepts PNGs up to 256 px and 64 KiB. Its visible
  # icon is smaller; retain the original artwork separately for store use.
  assert target.stat().st_size<=64*1024,'Device launcher icon exceeds 64 KiB'

for source,name,size in (('panel-source.png','panel','128x128!'),('button-source.png','button','256x64!')):
 target=root/'resources/ui'/f'{name}.png'
 original=root/'resources/brand'/source
 # Use alpha coverage for trim; invisible RGB and faint edge specks must not
 # leave empty bands inside a scalable button/frame.
 bounds=subprocess.check_output(['magick',str(original),'-alpha','extract','-threshold','50%','-format','%@','info:'],text=True).strip()
 subprocess.run(['magick',str(original),'-crop',bounds,'+repage','-resize',size,'-strip',str(target)],check=True)
 print(target)

subprocess.run(['magick',str(root/'resources/brand/title-background-source.png'),'-resize','180x320^','-gravity','center','-extent','180x320','-alpha','off','-strip',str(root/'resources/ui/title-background.png')],check=True)
