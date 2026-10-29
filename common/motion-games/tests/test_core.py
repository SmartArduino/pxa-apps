#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
import argparse,subprocess
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
exe=a.output/'core-test'
subprocess.run(['c++','-std=c++20','-O1','-g','-fsanitize=address,undefined',str(Path(__file__).with_name('core_test.cpp')),'-o',str(exe)],check=True)
subprocess.run([str(exe)],check=True)
