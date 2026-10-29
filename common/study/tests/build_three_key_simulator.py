#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Link a separate simulator with the actual Gen4 three-key adapter for tests."""
import argparse
from pathlib import Path
import shlex
import subprocess

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--workspace',type=Path,required=True)
p.add_argument('--gen4',type=Path,required=True)
p.add_argument('--output',type=Path,required=True)
p.add_argument('--private-quota-bytes',type=int,help='Override the test Host private-file quota (e.g. 65536 for Gen4)')
a=p.parse_args()
r=a.workspace.resolve();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
build=r/'build/simulator/doit-gamekit-1';src=r/'deps/pxa-system/simulator/desktop'
flags={}
for line in (build/'CMakeFiles/pxsys_product_simulator.dir/flags.make').read_text().splitlines():
    if line.startswith(('C_DEFINES =','C_INCLUDES =','C_FLAGS =')):
        key,value=line.split('=',1);flags[key.strip()]=shlex.split(value)
common=flags['C_DEFINES']+flags['C_INCLUDES']
text=(src/'product_runner.c').read_text()
if a.private_quota_bytes is not None:
    assert a.private_quota_bytes > 0
    import re
    text,n=re.subn(r'posix_fs_config\.quota_bytes\s*=\s*[^;]+;',f'posix_fs_config.quota_bytes = {a.private_quota_bytes}u;',text)
    assert n==1,'Simulator private-file quota declaration changed'
needle='uint32_t delay = lv_timer_handler();'
assert text.count(needle)==1,'Simulator loop changed; review test hook'
(out/'runner.c').write_text('void study_navigation_poll(void);\n'+text.replace(needle,'study_navigation_poll();\n        '+needle))
subprocess.run(['cc',*common,*flags['C_FLAGS'],'-I'+str(src),'-c',str(out/'runner.c'),'-o',str(out/'runner.o')],check=True)
subprocess.run(['c++',*common,'-std=c++20','-O2','-I'+str(a.gen4.resolve()/'components/mz79_navigation/include'),'-c',str(Path(__file__).with_name('navigation_bridge.cpp')),'-o',str(out/'navigation.o')],check=True)
command=shlex.split((build/'CMakeFiles/pxsys_product_simulator.dir/link.txt').read_text())
command=[x for x in command if not x.startswith('-Wl,--dependency-file=')]
index=command.index('-o');command[index+1]=str(out/'pxsys_product_simulator')
command=[str(out/'runner.o') if x.endswith('pxsys_product_simulator.dir/product_runner.c.o') else x for x in command]
command.extend([str(out/'navigation.o'),'-lstdc++'])
subprocess.run(command,cwd=build,check=True)
print(out/'pxsys_product_simulator')
