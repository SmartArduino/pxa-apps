#!/usr/bin/env python3
"""Compile regression TS with an external, pinned cartconv toolchain."""
import argparse
import importlib.util
import json
import os
from pathlib import Path
import shutil
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--cartconv',type=Path,required=True)
p.add_argument('--output',type=Path,required=True)
a=p.parse_args();a.cartconv=a.cartconv.resolve();a.output=a.output.resolve()
spec=importlib.util.spec_from_file_location('arcade_cartconv',a.cartconv/'tools/png2pxt64.py')
compiler=importlib.util.module_from_spec(spec);spec.loader.exec_module(compiler)
a.output.mkdir(parents=True,exist_ok=True)
# The tests consume bytecode, not the compiler's separate native SDL host.
# Supply the same no-native-build shim as cartconv, including on Unix hosts.
fakebin=a.output/'fakebin';fakebin.mkdir(exist_ok=True)
docker=fakebin/'docker';docker.write_text('#!/bin/sh\nexit 0\n');docker.chmod(0o755)
environment={'PATH':str(fakebin)+os.pathsep+os.environ.get('PATH','')}

template=None
for source in sorted((Path(__file__).resolve().parents[1]/'tests/cartridges').glob('*.ts')):
    project=a.cartconv/'vm-build/node_modules/pxt-arcade/.pxa-tests'/source.stem;project.mkdir(parents=True,exist_ok=True)
    shutil.copyfile(source,project/'main.ts')
    (project/'pxt.json').write_text(json.dumps({'name':source.stem,'dependencies':{'device':'*'},'files':['main.ts']}))
    if template:
        shutil.copytree(template/'pxt_modules',project/'pxt_modules',dirs_exist_ok=True)
        shutil.copytree(template/'built/dockercross',project/'built/dockercross',dirs_exist_ok=True)
    elif not (project/"pxt_modules/device/pxt.json").exists():
        installed=compiler.run_pxt(['install','--hwvariant','vm'],str(project),environment)
        if installed.returncode:raise RuntimeError(installed.stdout+installed.stderr)
    compiler.run_pxt(['build','--hwvariant','vm'],str(project),environment)
    if not (project/'built/binary.pxt64').exists():
        compiler.ensure_docker_stub(str(project))
        built=compiler.run_pxt(['build','--hwvariant','vm'],str(project),environment)
        if built.returncode:raise RuntimeError(built.stdout+built.stderr)
    shutil.copyfile(project/'built/binary.pxt64',a.output/(source.stem+'.pxt64'))
    template=project
    print(source.stem,flush=True)
