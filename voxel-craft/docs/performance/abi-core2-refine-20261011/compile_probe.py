from pathlib import Path
import shlex,subprocess,sys
root=Path.cwd();b=root/'local/abi-audit-20261011/simulator';out=root/'local/abi-refine-20261011';target=b/'CMakeFiles/pxsys_raster_test.dir';flags={}
for line in (target/'flags.make').read_text().splitlines():
 if ' = ' in line:
  k,v=line.split(' = ',1);flags[k]=shlex.split(v)
mode=sys.argv[1]
link=shlex.split((target/'link.txt').read_text());obj=out/(mode+'.o');binary=out/mode
extra=[]
if mode=='raster-asan':
 source=root/'deps/pxa-system/simulator/desktop/tests/raster_backend_test.c';extra=['-fsanitize=address,undefined','-fno-omit-frame-pointer','-g','-O1']
else:
 source=out/'voxel_probe.c';runner=out/'baseline-sdk/simulator/desktop/product_runner.c' if mode=='probe-baseline' else root/'deps/pxa-system/simulator/desktop/product_runner.c'
 extra=['-DPXA_PRODUCT_RUNNER="'+str(runner)+'"']
subprocess.run([link[0],*flags['C_DEFINES'],*flags['C_INCLUDES'],*flags['C_FLAGS'],*extra,'-c',str(source),'-o',str(obj)],check=True)
link=[str(obj) if x.endswith('raster_backend_test.c.o') else x for x in link if not x.startswith('-Wl,--dependency-file=')];link[link.index('-o')+1]=str(binary)
subprocess.run([*link,*extra],cwd=b,check=True)
