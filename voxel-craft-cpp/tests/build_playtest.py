"""Build a deterministic fixture under an isolated identity, never the real save data."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess

root = next((p for p in Path(__file__).resolve().parents
             if (p / 'tools/app.sh').is_file()), None)
if root is None:
    raise SystemExit('Run from a pxa-projects workspace checkout.')
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--target', choices=['esp32s3', 'simulator'], required=True)
parser.add_argument('--output', type=Path, required=True)
parser.add_argument('--tools', action='store_true',help='Use the isolated six-tool crafting/efficiency fixture')
parser.add_argument('--identity',default='pxa-voxel-craft-cpp-ui2',help='Isolated app ID; use a fresh ID for first-grant tests')
parser.add_argument('--font-size',type=int,choices=[10,14,18],help='Package only this font for a fixed-device fixture; production keeps all fonts')
args = parser.parse_args()
output = args.output.resolve()
source = output / 'source'
source.mkdir(parents=True, exist_ok=True)
app = source / 'voxel-craft-cpp'
if app.exists():
    shutil.rmtree(app)
shutil.copytree(Path(__file__).resolve().parents[1], app,
                ignore=shutil.ignore_patterns('docs', '*.cpp.txt', '*preview.png', '__pycache__'))
for name in ['common', '.dev-signing']:
    link = source / name
    if not link.exists():
        link.symlink_to(root / 'local/pxa-apps' / name, target_is_directory=True)
manifest = app / 'package.json'
package = json.loads(manifest.read_text())
if args.identity=='pxa-voxel-craft-cpp' or not args.identity.startswith('pxa-voxel-craft-cpp-ui'):
    parser.error('Fixture identity must be isolated from production')
package.update(id=args.identity, name='Voxel UI Test', release_sequence=4)
manifest.write_text(json.dumps(package, indent=2) + '\n')
if args.font_size:
    resources=app/'resources.json'
    description=json.loads(resources.read_text())
    description['assets']=[asset for asset in description['assets']
                           if 'menu-font-'not in asset['path']or f'menu-font-{args.font_size}.'in asset['path']]
    resources.write_text(json.dumps(description,indent=2)+'\n')
environment = dict(os.environ, PXA_APP_DEFINES='VOXEL_VALIDATE=1,VOXEL_PLAYTEST_SCENE=1'+(',VOXEL_PLAYTEST_TOOLS=1'if args.tools else''))
subprocess.run(['bash', 'tools/app.sh', 'build', 'voxel-craft-cpp', '--source-root', str(source),
                '--target', args.target, '--aot-only', '--output', str(output / 'artifacts')],
               cwd=root, env=environment, check=True)
