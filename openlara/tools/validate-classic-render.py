#!/usr/bin/env python3
"""Compare fixed cameras with the pinned original OpenGL and software engines."""
import argparse
import json
from pathlib import Path
import shlex
import subprocess
from PIL import Image, ImageDraw

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--upstream',type=Path,required=True)
p.add_argument('--data',type=Path,required=True,help='original demo LEVEL2.PHD')
p.add_argument('--output',type=Path,required=True)
a=p.parse_args();a.upstream=a.upstream.resolve();a.data=a.data.resolve();a.output=a.output.resolve()
a.output.mkdir(parents=True,exist_ok=True)
app=Path(__file__).resolve().parents[1];source=app/'tools/classic-render-reference.cpp'
pinned='3b268ca4bf986c380f44a26ffd9218bf6d376249'
assert subprocess.check_output(['git','-C',str(a.upstream),'rev-parse','HEAD'],text=True).strip()==pinned
assert not subprocess.check_output(['git','-C',str(a.upstream),'diff','--name-only','HEAD','--','src'],text=True).strip(),'reference source modified'
libraries=[a.upstream/'src/libs/stb_vorbis/stb_vorbis.c',a.upstream/'src/libs/minimp3/minimp3.cpp',a.upstream/'src/libs/tinf/tinflate.c']
report={'upstream':pinned,'resolution':[320,240],'scenes':{},'passed':False,
        'scope':'geometry and occlusion comparison; palette, affine texture mapping and lighting are not pixel-identical to OpenGL'}
for variant in ['gl','before','after']:
    directory=a.output/variant;directory.mkdir(exist_ok=True)
    include=app/'vendor/classic' if variant=='after' else a.upstream/'src'
    command=['c++','-std=c++11','-O1','-fwrapv','-fno-strict-aliasing','-fno-exceptions','-fno-rtti','-I'+str(include)]
    if variant=='gl':
        command+=['-D__SDL2__','-DSDL2_GLES','-D_GAPI_GLES2','-DOL_REFERENCE_GL=1']
    else:command+=['-D_GAPI_SW']
    command+=[str(source),*map(str,libraries),'-o',str(a.output/('reference-'+variant))]
    if variant=='gl':command+=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','sdl2','glesv2','egl'],text=True))
    command+=['-lpthread','-lm','-lrt']
    with (directory/'build.log').open('w') as log:subprocess.run(command,stdout=log,stderr=log,check=True)
    with (directory/'run.log').open('w') as log:subprocess.run([str(a.output/('reference-'+variant)),str(a.data),str(directory)],cwd=directory,stdout=log,stderr=log,check=True)
    poses=[line for line in (directory/'run.log').read_text().splitlines() if line.startswith(('POSE ','VIEW '))]
    if variant=='gl':reference_poses=poses
    else:assert poses==reference_poses,(variant,'camera/input mismatch')
    report[variant+'_poses']=poses
canvas=Image.new('RGB',(960,1350));draw=ImageDraw.Draw(canvas)
for row,name in enumerate(['start','turned','walked','reverse','doorway']):
    images={}
    for col,variant in enumerate(['gl','before','after']):
        im=Image.open(a.output/variant/(name+'.ppm')).convert('RGB');images[variant]=im
        im.save(a.output/variant/(name+'.png'))
        draw.text((col*320+8,row*270+8),name+': '+{'gl':'Original OpenGL','before':'Before','after':'Depth + clipping'}[variant],fill='white')
        canvas.paste(im,(col*320,row*270+30))
    counts={variant:sum(max(original)>8 and max(pixel)<2 for original,pixel in zip(images['gl'].getdata(),images[variant].getdata())) for variant in ['before','after']}
    report['scenes'][name]={'black_pixels_where_original_has_geometry':counts}
canvas.save(a.output/'comparison.png')
# The reverse camera exposed a large missing wall in the original SW backend.
counts=report['scenes']['reverse']['black_pixels_where_original_has_geometry']
assert counts['after']<counts['before']//10,'near wall still missing'
report['passed']=True
(a.output/'metrics.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
