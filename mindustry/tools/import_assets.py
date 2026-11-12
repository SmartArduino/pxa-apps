#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Reproduce the small, original Mindustry sprite/sound subset (Pillow required)."""
from pathlib import Path
import concurrent.futures, hashlib, json, time, urllib.request, subprocess
from PIL import Image
from building_art import compose
ROOT=Path(__file__).resolve().parents[1]
COMMIT='d1e0307d3906bb350e80d8969a2464ed0b771ffb' # v154
BASE=f'https://raw.githubusercontent.com/Anuken/Mindustry/{COMMIT}/'
SPRITES={
 'icon_production':'@icons/production',
 'icon_distribution':'@icons/distribution',
 'icon_turret':'@icons/turret',
 'icon_crafting':'@icons/crafting',
 'icon_power':'@icons/power',
 'icon_liquid':'@icons/liquid',
 'icon_units':'@icons/units',
 'icon_pause':'@icons/pause',
 'icon_menu':'@icons/menu',
 'icon_check':'@icons/check',
 'icon_cancel':'@icons/cancel',
 'icon_rotate':'@icons/rotate',
 'icon_tree':'@icons/tree',
 'icon_eraser':'@icons/eraser',
 'icon_undo':'@icons/undo',
 'icon_command_rally':'@icons/command-rally',
 'icon_command_attack':'@icons/command-attack',
 'factoryout':'blocks/payload/factory-out-3',
 'factorytop':'blocks/payload/factory-top-3',
 'turretbase':'blocks/turrets/bases/block-1',
 'batterytop':'blocks/power/battery-top',
 'conduiticonbottom':'blocks/liquid/conduits/conduit-bottom',
 'liquidrouterbottom':'blocks/liquid/liquid-router-bottom',
 'tankbottom':'blocks/liquid/liquid-tank-bottom',
 'pump':'blocks/liquid/mechanical-pump', 'conduit':'blocks/liquid/conduits/conduit-top-0',
 'conduitbottom':'blocks/liquid/conduits/conduit-bottom-0',
 'liquidrouter':'blocks/liquid/liquid-router','tank':'blocks/liquid/liquid-tank',
 'groundfactory':'blocks/units/ground-factory','airfactory':'blocks/units/air-factory',
 'flare':'units/flare','kiln':'blocks/production/kiln',
 'titanium':'blocks/environment/ore-titanium1','itemtitanium':'items/item-titanium',
 'itemglass':'items/item-metaglass','snow':'blocks/environment/snow1',
 'stone1':'blocks/environment/stone1', 'stone2':'blocks/environment/stone2', 'stone3':'blocks/environment/stone3',
 'sand1':'blocks/environment/sand-floor1', 'water':'blocks/environment/shallow-water',
 'copper':'blocks/environment/ore-copper1','lead':'blocks/environment/ore-lead1','coal':'blocks/environment/ore-coal1',
 'rock':'blocks/environment/stone-wall1',
 'core':'blocks/storage/core-shard','coreteam':'blocks/storage/core-shard-team',
 'drill':'blocks/drills/mechanical-drill','drillrotor':'blocks/drills/mechanical-drill-rotator','drilltop':'blocks/drills/mechanical-drill-top',
 'pneumatic':'blocks/drills/pneumatic-drill',
 'pneumaticrotor':'blocks/drills/pneumatic-drill-rotator',
 'pneumatictop':'blocks/drills/pneumatic-drill-top',
 'drillitem':'blocks/drills/drill-item-2',
 'belt':'blocks/distribution/conveyors/conveyor-0-0','belt1':'blocks/distribution/conveyors/conveyor-0-1',
 'router':'blocks/distribution/router','junction':'blocks/distribution/junction',
 'duo':'blocks/turrets/duo/duo-preview','arc':'blocks/turrets/arc','wall':'blocks/walls/copper-wall',
 'press':'blocks/production/graphite-press','smelter':'blocks/production/silicon-smelter',
 'generator':'blocks/power/combustion-generator','node':'blocks/power/power-node','battery':'blocks/power/battery',
 'mender':'blocks/defense/mender','dagger':'units/dagger','crawler':'units/crawler','alpha':'units/alpha',
 'itemcopper':'items/item-copper','itemlead':'items/item-lead','itemcoal':'items/item-coal','itemgraphite':'items/item-graphite',
 'itemsilicon':'items/item-silicon','itemsand':'items/item-sand',
}
# v154 Conveyor uses five connection shapes and four synchronized frames.
for shape in range(5):
 for frame in range(4):
  if shape == 0 and frame < 2:continue
  SPRITES[f'belt_{shape}_{frame}']=f'blocks/distribution/conveyors/conveyor-{shape}-{frame}'
# Fallbacks are explicit upstream paths, never generated replacement artwork.
SPRITES['sand1']='blocks/environment/sand-floor1'
SOUNDS={'click':'ui/uiButton','wave':'ui/waveSpawn','shoot':'shoot/shootDuo','explode':'explosions/blockExplode1'}
def fetch(pair):
 name,path=pair; dest=ROOT/'assets/upstream'/name
 if not dest.exists():
  for attempt in range(4):
   try:
    data=urllib.request.urlopen(BASE+path,timeout=45).read();dest.write_bytes(data);break
   except Exception:
    if attempt==3:raise
    time.sleep(attempt+1)
 return {'file':str(dest.relative_to(ROOT)),'source':path,'sha256':hashlib.sha256(dest.read_bytes()).hexdigest()}
def main():
 pairs=[(n+'.png',('core/assets-raw/'+p[1:] if p.startswith('@') else 'core/assets-raw/sprites/'+p)+'.png') for n,p in SPRITES.items()]
 pairs += [(n+'.ogg','core/assets/sounds/'+p+'.ogg') for n,p in SOUNDS.items()]
 pairs += [('LICENSE.txt','LICENSE'),('app-icon.png','ios/data/Assets.xcassets/AppIcon.appiconset/icon-256.png')]
 with concurrent.futures.ThreadPoolExecutor(max_workers=8) as pool:
  records=list(pool.map(fetch,pairs))
 (ROOT/'upstream.json').write_text(json.dumps({'repository':'https://github.com/Anuken/Mindustry','revision':COMMIT,'tag':'v154','license':'GPL-3.0-only','files':records},indent=2)+'\n')
 for name in SOUNDS:
  subprocess.run(['ffmpeg','-v','error','-y','-i',str(ROOT/'assets/upstream'/f'{name}.ogg'),'-ac','1','-ar','16000','-f','s16le',str(ROOT/'assets'/f'{name}.s16')],check=True)
 out=['// Generated by tools/import_assets.py from attributed GPL-3.0 Mindustry assets.','#pragma once','#include <cstdint>','namespace art {','struct Sprite {int w,h; const uint32_t *p;};']
 images=compose({n:Image.open(ROOT/'assets/upstream'/f'{n}.png').convert('RGBA') for n in SPRITES})
 for n,im in images.items():
  size=48 if n in ('core','coreteam','tank','factoryout','factorytop','tankbottom') or n.startswith(('groundfactory','airfactory')) else 32 if n.startswith(('drill','pneumatic')) or n in ('press','smelter','kiln') else 24 if n in ('alpha','dagger','flare','crawler') else 16
  im.thumbnail((size,size),Image.Resampling.NEAREST)
  data=[]
  for r,g,b,a in im.getdata(): data.append(0 if a<100 else 0x10000|((r&248)<<8)|((g&252)<<3)|(b>>3))
  out += [f'inline constexpr uint32_t {n}_pixels[]={{'+','.join(hex(v) for v in data)+'};',f'inline constexpr Sprite {n}{{{im.width},{im.height},{n}_pixels}};']
 out.append('inline constexpr const Sprite *belt_frames[5][4]={')
 for shape in range(5):
  names=[('belt' if frame==0 else 'belt1') if shape==0 and frame<2 else f'belt_{shape}_{frame}' for frame in range(4)]
  out.append('{'+','.join('&'+n for n in names)+'},')
 out.append('};')
 for kind in ('drill','pneumatic'):
  out.append(f'inline constexpr const Sprite *{kind}_frames[]={{'+','.join(f'&{kind}_frame{i}' for i in range(8))+'};')
 for kind in ('groundfactory','airfactory'):
  out.append(f'inline constexpr const Sprite *{kind}_frames[]={{&{kind},'+','.join(f'&{kind}_dir{d}' for d in range(1,4))+'};')
 out.append('struct UnitSprite {const Sprite &body;int sourceWidth,sourceHeight;};')
 for n in ('alpha','dagger','flare','crawler'):
  # World sizing uses original pixels (32 per tile), independently of the
  # packed thumbnail resolution. Keep transparent margins and aspect ratio.
  w,h=Image.open(ROOT/'assets/upstream'/f'{n}.png').size
  out.append(f'inline constexpr UnitSprite {n}_unit{{{n},{w},{h}}};')
 out+=['}'];(ROOT/'art.hpp').write_text('\n'.join(out)+'\n')
 (ROOT/'assets/icon.png').write_bytes((ROOT/'assets/upstream/app-icon.png').read_bytes())
 print(f'Imported {len(records)} pinned original assets')
if __name__=='__main__':main()
