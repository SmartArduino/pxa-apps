"""Compile and execute only the original-software-raster/SDL2 comparison."""
from pathlib import Path
import argparse,hashlib,json,os,re,shlex,subprocess,time,zlib
R=Path(__file__).resolve().parents[1];U=R/'vendor/original';P=R
def pin(p):
 b=p.read_bytes();return dict(path=str(p),bytes=len(b),sha256=hashlib.sha256(b).hexdigest())
p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);a=p.parse_args()
OUT=a.output.resolve();OUT.mkdir(parents=True,exist_ok=True);(OUT/'tmp').mkdir(exist_ok=True)
gfx=(U/'vgagrafx.c').read_text()
arrays={m.group(1):len(re.findall(r'0x[0-9a-fA-F]+|\b\d+\b',re.sub(r'/\*.*?\*/','',m.group(2),flags=re.S)))
 for m in re.finditer(r'static const uint8_t\s+(\w+)\[\]\s*=\s*\{(.*?)\};',gfx,re.S)}
table=re.search(r'const uint8_t \*vgatable\[\]\s*=\s*\{(.*?)\};',gfx,re.S)
names=re.findall(r'\b[A-Za-z_]\w*\b',re.sub(r'/\*.*?\*/','',table.group(1),flags=re.S))
assert len(names)==240
dimensions={960:(4,15),896:(4,14),768:(4,12),640:(4,10),576:(3,12),320:(5,4),256:(2,8)}
dims=[dimensions[min(arrays[names[i]],arrays[names[i+1]])] for i in range(0,240,2)]
title_source=(U/'title_gz.c').read_text()
blob=re.search(r'title_gz\s*\[[^]]*\]\s*=\s*\{(.*?)\};',title_source,re.S).group(1)
compressed=bytes(int(n,0) for n in re.findall(r'0x[0-9a-fA-F]+|\b\d+\b',blob))
original_title=zlib.decompress(compressed)
assert len(compressed)==2772 and len(original_title)==256000
h='/* Local golden fixture derived byte-for-byte from pinned upstream. */\n'
h+='static const unsigned char raster_original_title[256000]={\n'
h+='\n'.join(','.join(str(v) for v in original_title[i:i+32])+',' for i in range(0,len(original_title),32))+'\n};\n'
h+='static const short raster_sprite_dims[120][2]={'+','.join('{%d,%d}'%d for d in dims)+'};\n'
(OUT/'raster-fixture.h').write_text(h)
(OUT/'reference-support.c').write_text('#include <SDL.h>\nvoid raster_reference_free(void *p){SDL_FreeSurface((SDL_Surface *)p);}\n')
functions=['vgainit','vgaclear','vgapal','vgainten','vgaputi','vgageti','vgagetpix','vgaputim','vgawrite','vgatitle','gretrace','savescreen','cgainit','cgaclear','cgatitle','cgawrite','cgaputim','cgageti','cgaputi','cgapal','cgainten','cgagetpix','doscreenupdate','switchmode','sdl_enable_fullscreen','digger_raster_reset','graphicsoff','digger_pixels640','digger_palette565']
sdl_cflags=shlex.split(subprocess.check_output(['pkg-config','--cflags','sdl2'],text=True))
sdl_libs=shlex.split(subprocess.check_output(['pkg-config','--libs','sdl2'],text=True))
sources=[P/'raster.c',P/'surface-lite.c',P/'raster-test.c',U/'vgagrafx.c',U/'alpha.c']
pins_before={str(f.relative_to(R)):pin(f) for f in sources}
env={**os.environ,'TMPDIR':str(OUT/'tmp'),'ASAN_OPTIONS':'detect_leaks=1'}
records=[]
def run(label,argv):
 start=time.time()
 with (OUT/(label+'.stdout')).open('wb') as o,(OUT/(label+'.stderr')).open('wb') as e:
  cp=subprocess.run([str(v) for v in argv],stdout=o,stderr=e,cwd=OUT,env=env)
 rec=dict(label=label,argv=[str(v) for v in argv],actual_exit_code=cp.returncode,elapsed_seconds=round(time.time()-start,3),stdout=pin(OUT/(label+'.stdout')),stderr=pin(OUT/(label+'.stderr')))
 records.append(rec)
 if cp.returncode: raise RuntimeError(label+' actual exit '+str(cp.returncode))
 return rec
failure=None
try:
 for mode,extra in [('o2',['-O2']),('asan-ubsan',['-O1','-g','-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer'])]:
  common=['gcc','-std=c11','-Wall','-Wextra','-Wno-unused-variable','-I'+str(U),'-I'+str(P),'-I'+str(OUT)]+extra
  ref=OUT/(mode+'-ref.o')
  run(mode+'-reference-compile',common+sdl_cflags+['-DDIGGER_RASTER_REFERENCE_SDL']+['-D'+n+'=ref_'+n for n in functions]+['-c',str(P/'raster.c'),'-o',str(ref)])
  exe=OUT/('raster-'+mode)
  run(mode+'-compile',common+sdl_cflags+[str(v) for v in sources]+[str(ref),str(OUT/'reference-support.c'),'-o',str(exe)]+sdl_libs)
  run(mode+'-run',[exe])
except Exception as exc:
 failure=str(exc)
after={str(f.relative_to(R)):pin(f) for f in sources}
report=dict(passed=failure is None,actual_exit_code=0 if failure is None else 1,reason=failure,commands=records,
 source_inventory_start=pins_before,source_inventory_end=after,sources_unchanged=pins_before==after,
 title_bytes=len(original_title),title_sha256=hashlib.sha256(original_title).hexdigest(),
 sprite_pairs=120,dead_hobbin80_test_height=14,original_dead_hobbin80_source_bytes=896,
 scope='Native original software algorithms with true SDL2 index8 reference; no window, Guest or signing.',
 Guest_executed=False,signed=False,old_workspace_modified=False)
(OUT/'result.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(dict(passed=report['passed'],actual_exit_code=report['actual_exit_code'],reason=failure,report=pin(OUT/'result.json'))))
raise SystemExit(report['actual_exit_code'])

