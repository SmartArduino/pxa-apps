from pathlib import Path
import subprocess,json,hashlib
r=Path.cwd();d=r/'local/game-render-opt3-20261015';p=d/'width-pixels';p.mkdir(exist_ok=True);shared=r/'local/game-render-opt2-20261015';n=r/'local/game-render-opt-20261014/native-games'
for name,src in [('old',d/'baseline-source/raster.c'),('new',d/'width-raster.c')]:
 dest=p/name;dest.mkdir(exist_ok=True);obj=p/('raster-'+name+'.o')
 subprocess.run(['clang','-std=c11','-O3','-fno-strict-aliasing','-I'+str(r/'deps/pxa-system/libpxa/include'),'-c',str(src),'-o',str(obj)],check=True)
 for kind,guest in [('render',d/'render_test.o'),('fixed',shared/'render_fixed.o')]:
  subprocess.run(['clang++','-O3',str(guest),str(shared/'world.o'),str(shared/'mesher.o'),str(obj),str(shared/'pixels/wire.o'),str(n/'libsdk.a'),'-o',str(p/(kind+'-'+name))],check=True)
  outdir=dest if kind=='render' else dest/'fixed';outdir.mkdir(exist_ok=True);subprocess.run([str(p/(kind+'-'+name)),str(outdir)],check=True)
 subprocess.run(['clang++','-O3',str(n/'CMakeFiles/jump_preview.dir/home/lucinhu/Documents/workspace/esp32/zuowei_paixueji_touch/pxa-projects/local/pxa-apps/jump-jump-3d/tools/preview.cpp.o'),str(n/'libjump_render.a'),str(obj),str(shared/'pixels/wire.o'),str(n/'libsdk.a'),str(n/'libjump_core.a'),'-o',str(p/('jump-'+name))],check=True)
 subprocess.run([str(p/('jump-'+name)),'--frames','3','--auto','8','--out',str(dest/'jump')],check=True,stdout=subprocess.DEVNULL)
out=[]
for a in sorted((p/'old').rglob('*')):
 if not a.is_file():continue
 relative=a.relative_to(p/'old');b=p/'new'/relative;assert a.read_bytes()==b.read_bytes(),str(relative);out.append({'name':str(relative),'bytes':a.stat().st_size,'sha256':hashlib.sha256(a.read_bytes()).hexdigest()})
assert len(out)==20,len(out);(d/'width-pixels.json').write_text(json.dumps({'passed':True,'outputs':out},indent=2)+'\n');print('Width pixel equality:',len(out))
s=(d/'check-painter.py').read_text().replace("d/'endpoint-raster.c'","d/'width-raster.c'").replace('fixture-endpoint','fixture-width').replace("d/'endpoint-fixture.json'","d/'width-fixture.json'");(d/'check-painter-width.py').write_text(s)
subprocess.run(['python3',str(d/'check-painter-width.py')],check=True)
