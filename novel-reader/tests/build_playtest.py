"""Build an instrumented reader under an isolated identity; never touch real books."""
import argparse, json, os, shutil, subprocess
from pathlib import Path
root=next(p for p in Path(__file__).resolve().parents if (p/'tools/app.sh').is_file())
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--target',choices=['simulator','esp32s3','esp32s31'],required=True)
p.add_argument('--output',type=Path,required=True)
p.add_argument('--source',help='Optional JSON fixture catalog URL')
p.add_argument('--identity',default='pxa-novel-reader-qa')
a=p.parse_args()
if not a.identity.startswith('pxa-novel-reader-qa'):p.error('Use an isolated QA identity')
base=a.output.resolve();source=base/'source';app=source/'novel-reader'
if app.exists():shutil.rmtree(app)
shutil.copytree(Path(__file__).resolve().parents[1],app,ignore=shutil.ignore_patterns('docs','__pycache__'))
for name in ['common','.dev-signing']:(source/name).symlink_to(root/'local/pxa-apps'/name,target_is_directory=True) if not (source/name).exists() else None
manifest=app/'package.json';m=json.loads(manifest.read_text());m.update(id=a.identity,name='书页验收',release_sequence=10);manifest.write_text(json.dumps(m,ensure_ascii=False,indent=2)+'\n')
if a.source:
 f=app/'reader_state.hpp';text=f.read_text();marker='std::vector<Source> sources{';assert marker in text;text=text.replace(marker,marker+'{"验收书源",'+json.dumps(a.source)+',SourceKind::json},\n        ',1);f.write_text(text)
 # Test the real source-import button without depending on a desktop keyboard.
 f=app/'reader_ui.inc';text=f.read_text();marker='case add_source:';at=text.index(marker);pos=text.index('input.set("");',at);text=text[:pos]+text[pos:].replace('input.set("");','input.set('+json.dumps(a.source.rsplit('/',1)[0]+'/source.json')+');',1);f.write_text(text)
env=dict(os.environ,PXA_APP_DEFINES='READER_VALIDATE=1')
subprocess.run(['bash','tools/app.sh','build','novel-reader','--target',a.target,'--source-root',str(source),'--aot-only','--output',str(base/'artifacts')],cwd=root,env=env,check=True)
