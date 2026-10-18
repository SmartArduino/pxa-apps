#!/usr/bin/env python3
"""Prepare complete-engine media, or the earlier fixed alpha with --engine fixed."""
from pathlib import Path
import runpy
import sys

args=sys.argv[1:]
engine='classic'
if '--engine' in args:
    i=args.index('--engine')
    if i+1==len(args) or args[i+1] not in ('classic','fixed'):
        raise SystemExit('--engine must be classic or fixed')
    engine=args[i+1]
    del args[i:i+2]
target=Path(__file__).with_name(f'prepare-{engine}-data.py')
sys.argv=[str(target),*args]
runpy.run_path(str(target),run_name='__main__')
