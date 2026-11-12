# SPDX-License-Identifier: GPL-3.0-only
"""v154 icon stacks, from Block.icons, DrawTurret, PayloadBlock and drawers.
Keep state-dependent flame, warmup, pump-liquid and repair glows out of icons.
"""
from PIL import Image

ICON_LAYERS = {
 'core': ('core', 'coreteam_sharded'),
 'drill': ('drill', 'drillrotor', 'drilltop'),
 'belt': ('belt',), 'router': ('router',), 'junction': ('junction',),
 'duo': ('turretbase', 'duo'), 'wall': ('wall',), 'press': ('press',),
 'smelter': ('smelter',), 'generator': ('generator',), 'node': ('node',),
 'battery': ('battery', 'batterytop'), 'mender': ('mender',),
 'arc': ('turretbase', 'arc'),
 'pneumatic': ('pneumatic', 'pneumaticrotor', 'pneumatictop'),
 'kiln': ('kiln',), 'pump': ('pump',),
 'conduit': ('conduiticonbottom', 'conduit'),
 'liquidrouter': ('liquidrouterbottom', 'liquidrouter'),
 'tank': ('tankbottom', 'tank'),
 'groundfactory': ('groundfactory', 'factoryout', 'factorytop'),
 'airfactory': ('airfactory', 'factoryout', 'factorytop'),
}
SOURCE_ONLY = {
 'drill','drillrotor','drilltop','pneumatic','pneumaticrotor','pneumatictop',
 'coreteam','conduitbottom','conduiticonbottom','liquidrouterbottom','tankbottom',
 'factoryout','factorytop','batterytop','turretbase',
}

def sharded_team(image):
 # Block.createIcons uses Team.sharded's three palette colors, not a uniform
 # color multiplication. Retain any pixels outside these original markers.
 colors={(255,255,255):(255,211,127),
         (220,198,198):(234,182,120),(219,197,197):(234,182,120),
         (157,127,127):(212,129,107),(158,128,128):(212,129,107)}
 result=image.copy()
 result.putdata([(*colors.get(p[:3],p[:3]),p[3]) for p in image.getdata()])
 return result

def stack(images):
 images=list(images)
 result=Image.new('RGBA',images[0].size)
 for image in images:
  if image.size!=result.size:raise ValueError(('building layer dimensions',image.size,result.size))
  result.alpha_composite(image)
 return result

def compose(raw):
 source=dict(raw);source['coreteam_sharded']=sharded_team(source['coreteam'])
 result={name:image.copy() for name,image in raw.items()}
 for name,layers in ICON_LAYERS.items():
  result[name]=stack(source[layer] for layer in layers)
 for kind,rotor,top in [('drill','drillrotor','drilltop'),('pneumatic','pneumaticrotor','pneumatictop')]:
  for phase in range(8):
   result[f'{kind}_frame{phase}']=stack([source[kind],source[rotor].rotate(-phase*90/8,resample=Image.Resampling.NEAREST),source[top]])
 for kind in ('groundfactory','airfactory'):
  for direction in range(1,4):
   # Only the outlet rotates in UnitFactory.drawPlanRegion/draw. Preserve the
   # fixed lighting and frame orientation of the base and factory top.
   result[f'{kind}_dir{direction}']=stack([source[kind],source['factoryout'].rotate(direction*90),source['factorytop']])
 return result
