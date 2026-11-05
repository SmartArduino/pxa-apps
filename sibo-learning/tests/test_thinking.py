#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Bounded offline maths checked against independent exhaustive oracles."""
import argparse,subprocess
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
app=Path(__file__).resolve().parents[1];exe=a.output/'thinking-test'
subprocess.run(['c++','-std=c++20','-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer',str(app/'thinking.cpp'),str(app/'tests/thinking_test.cpp'),'-o',str(exe)],check=True)
subprocess.run([str(exe)],check=True,timeout=120)
