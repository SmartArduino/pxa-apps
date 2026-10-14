from pathlib import Path
import subprocess,hashlib,json
r=Path.cwd();d=r/'local/game-render-opt3-20261015';out=[]
for name,source in [('baseline',d/'baseline-source/raster.c'),('candidate',d/'width-raster.c')]:
 exe=d/('fixture-width-'+name)
 subprocess.run(['clang','-std=c11','-O3','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-strict-aliasing','-I'+str(r/'deps/pxa-system/libpxa/include'),'-DRASTER_SOURCE="'+str(source)+'"',str(d/'painter-fixture.c'),str(r/'deps/pxa-system/libpxa/src/core/wire.c'),'-o',str(exe)],check=True)
 with (d/('fixture-width-'+name+'.log')).open('w') as log:
  p=subprocess.Popen([str(exe)],stdout=subprocess.PIPE,stderr=log);sha=hashlib.sha256();size=0
  while chunk:=p.stdout.read(65536):sha.update(chunk);size+=len(chunk)
  assert p.wait()==0,name
 out.append({'name':name,'sha256':sha.hexdigest(),'bytes':size})
assert out[0]['sha256']==out[1]['sha256'],out
(d/'width-fixture.json').write_text(json.dumps({'passed':True,'cases':2400,'variants':out},indent=2)+'\n');print(out)
