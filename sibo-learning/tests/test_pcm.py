#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Sanitized checks of duration, channel mixing and streaming resampling."""
import argparse
from pathlib import Path
import subprocess
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output',type=Path,required=True)
a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
app=Path(__file__).resolve().parents[1];root=app.parents[2];exe=a.output/'pcm-test'
subprocess.run(['c++','-std=c++20','-O0','-g','-fsanitize=address,undefined','-I'+str(root/'deps/pxa-system/sdk/guest-c/include'),str(app/'tests/pcm_test.cpp'),'-o',str(exe)],check=True)
subprocess.run([str(exe)],check=True,timeout=30)
