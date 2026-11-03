#!/usr/bin/env python3
"""Check bounded layouts, input ownership, settings and system glyph caching."""
import argparse
from pathlib import Path
import subprocess

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--output',type=Path,required=True)
a=p.parse_args()
out=a.output.resolve()
if '/local/artifacts/' not in str(out):
    p.error('Use an output directory under local/artifacts/')
out.mkdir(parents=True,exist_ok=True)
app=Path(__file__).resolve().parents[1]
root=app.parents[2]
base=['cc','-std=gnu11','-g','-O1','-Wall','-Wextra',
      '-Wno-attributes','-Wno-misleading-indentation',
      '-fsanitize=address,undefined','-fno-omit-frame-pointer',
      '-ffunction-sections','-fdata-sections','-I'+str(app),
      '-I'+str(app/'upstream/include'),'-I'+str(app.parent/'common/recrof'),
      '-I'+str(root/'deps/pxa-system/sdk/guest-c/include')]
for name,units,args in [
    ('frontend-model',['layout.c','settings.c','input.c','controls.c','tests/frontend_model_test.c'],['.']),
    ('font-cache',['tests/font_cache_test.c'],[])]:
    subprocess.run(base+[str(app/u) for u in units]+['-Wl,--gc-sections','-o',str(out/name)],check=True)
    subprocess.run([str(out/name)]+args,check=True,cwd=out)
print('PASS: ASAN/UBSAN frontend and font cache regression')
