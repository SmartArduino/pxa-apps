#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Check learning schemas and malformed responses under address/UB sanitizers."""
import argparse
from pathlib import Path
import subprocess
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--output',type=Path,required=True)
p.add_argument('--fixtures',type=Path)
a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
app=Path(__file__).resolve().parents[1]
exe=a.output/'learn-test'
subprocess.run(['c++','-std=c++20','-g','-O0','-fsanitize=address,undefined','-fno-omit-frame-pointer',str(app/'model.cpp'),str(app/'learn.cpp'),str(app/'tests/learn_test.cpp'),'-o',str(exe)],check=True)
subprocess.run([str(exe)]+([str(a.fixtures.resolve())] if a.fixtures else []),check=True)
