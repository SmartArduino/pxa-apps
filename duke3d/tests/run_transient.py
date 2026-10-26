#!/usr/bin/env python3
"""Check transient checkpoints without changing durable save-slot semantics."""
import argparse
from pathlib import Path
import subprocess
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--output',type=Path,required=True)
args=p.parse_args();out=args.output.resolve();out.mkdir(parents=True,exist_ok=True)
source=Path(__file__).resolve().parents[1]
subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-g','-O1',
    '-fsanitize=address,undefined','-fno-omit-frame-pointer',
    '-I'+str(source),'-I'+str(source.parent/'common/recrof'),
    str(source/'transient_nvm.c'),str(source/'tests/transient_nvm_test.c'),
    '-o',str(out/'test')],check=True)
subprocess.run([str(out/'test')],check=True)
