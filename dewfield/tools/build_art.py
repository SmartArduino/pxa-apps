#!/usr/bin/env python3
"""Compile the original title illustration into bounded RGB565 game data."""
from pathlib import Path
from PIL import Image
import hashlib,json
root=Path(__file__).resolve().parents[1]
src=root/'art/title-source.png'
im=Image.open(src).convert('RGB')
small=im.resize((240,160),Image.Resampling.NEAREST)
colors=[((r&248)<<8)|((g&252)<<3)|(b>>3) for r,g,b in small.getdata()]
(root/'title_art.hpp').write_text('// Original AI-generated Dewfield illustration. See art/title-source.json.\n#pragma once\n#include <cstdint>\nnamespace title_art {\ninline constexpr int Width=240,Height=160;\ninline constexpr uint16_t pixels[]={\n'+''.join(','.join(map(str,colors[i:i+24]))+',\n' for i in range(0,len(colors),24))+'};\n}\n')
w,h=im.size
im.crop((0,0,h,h)).resize((128,128),Image.Resampling.NEAREST).save(root/'assets/icon.png')
(root/'art/title-source.json').write_text(json.dumps({'tool':'built-in imagegen','source':'title-source.png','sha256':hashlib.sha256(src.read_bytes()).hexdigest(),'runtime_size':[240,160],'conversion':'RGB565, nearest-neighbor, no runtime decoding','prompt':'Use case: game-asset. Asset type: original pixel art title-screen illustration for a handheld farming game called Dewfield. Create a landscape 3:2 composition. Warm nostalgic 16-bit SNES top-down rural farming aesthetic inspired by the gentle countryside mood of Stardew Valley, but all buildings, characters and layout are original. A tiny amber timber cottage with a terracotta roof to upper left, golden winding footpath, lush layered oak trees, neat vegetable beds with radishes and pumpkins, a blue pond at lower right with wooden dock, white chickens, distant blue-green mountains under a pale mint morning sky. Crisp deliberately placed chunky square pixels, rich warm ochre earth, sage and olive foliage, light cream highlights, detailed organic leaf clusters and beautiful rustic architecture. Quiet pastoral inviting mood. Leave the top center sky open for a title that will be drawn by game code. No text, no logo, no watermark, no UI, no border. This is real in-game title artwork, not a mockup or screenshot.'},ensure_ascii=False,indent=2)+'\n')
