#!/usr/bin/env python3
"""Build the read-only memory observer from an existing PXA simulator test tree.

Does not edit/reconfigure the simulator or link over any existing executable.
First build target pxsys_resource_app_test with the project's normal build.
"""
import argparse,json,shlex,subprocess
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--build',type=Path,required=True)
p.add_argument('--runner',type=Path,required=True)
p.add_argument('--output',type=Path,required=True)
a=p.parse_args();build=a.build.resolve();output=a.output.resolve();output.parent.mkdir(parents=True,exist_ok=True)
target=build/'CMakeFiles/pxsys_resource_app_test.dir'
obj=output.with_suffix('.o')
source=Path(__file__).with_name('memory_probe.c').resolve()
if (target/'flags.make').exists():
 flags={}
 for line in (target/'flags.make').read_text().splitlines():
  if line.startswith(('C_DEFINES =','C_INCLUDES =','C_FLAGS =')):
   key,value=line.split('=',1);flags[key.strip()]=shlex.split(value)
 compile=['/usr/bin/cc',*flags['C_DEFINES'],*flags['C_INCLUDES'],*flags['C_FLAGS'],'-c',str(source),'-o',str(obj)]
 link=shlex.split((target/'link.txt').read_text())
else:
 entries=json.loads((build/'compile_commands.json').read_text())
 entry=next(row for row in entries if row['file'].endswith('/tests/resource_app_test.c'))
 compile=entry.get('arguments') or shlex.split(entry['command'])
 compile[compile.index('-o')+1]=str(obj)
 compile[compile.index('-c')+1]=str(source)
 # Ninja's no-op shell wrappers are not arguments to the linker.
 commands=subprocess.check_output(['ninja','-t','commands','pxsys_resource_app_test'],cwd=build,text=True)
 link=shlex.split(commands.splitlines()[-1])
 if link[:2]==[':','&&']:link=link[2:]
 if link[-2:]==['&&',':']:link=link[:-2]
 assert not any(token in ('&&',';','||') for token in link)
compile+=['-DPXA_PRODUCT_RUNNER="'+str(a.runner.resolve())+'"']
subprocess.run(compile,cwd=build,check=True)
# Existing test objects provide networking and the control socket. Only replace
# its main/runner translation unit, with the same headers and library versions.
for i,arg in enumerate(link):
 if arg.endswith('/tests/resource_app_test.c.o'):link[i]=str(obj)
 if arg.startswith('-Wl,--dependency-file='):link[i]='-Wl,--dependency-file='+str(output)+'.d'
 if arg=='-o':link[i+1]=str(output)
subprocess.run(link,cwd=build,check=True)
print(output)
