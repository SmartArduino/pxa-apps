#!/usr/bin/env python3
"""Hermetic build-tool regression: identities, optional use and safe publication."""
import argparse
import copy
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('generator',type=Path)
a=p.parse_args()
with tempfile.TemporaryDirectory(prefix='pxa-script-library-test-') as directory:
    app=Path(directory)/'app'
    for sub in ('tools','runtime','assets/script-libraries','vendor'):(app/sub).mkdir(parents=True,exist_ok=True)
    generator=app/'tools/compile_libraries.py';shutil.copyfile(a.generator,generator)
    compiler=app/'compiler';compiler.write_text('''#!/usr/bin/env python3
from pathlib import Path
import sys
raw=Path(sys.argv[1]).read_bytes()
if raw==b'FAIL':sys.exit(3)
output=b'bad' if raw==b'BAD' else b'PXAQJS1\\n'+raw
if raw==b'LARGE':output=b'PXAQJS1\\n'+b'x'*65536
Path(sys.argv[2]).write_bytes(output)
''');compiler.chmod(0o700)
    manifest={'format':'pxa-browser-script-libraries-1','libraries':[{'id':'example-1','source':'vendor/example.js','license':'vendor/LICENSE','filename':'example.js'}]}
    (app/'vendor/example.js').write_bytes(b'function example(){return 1}')
    (app/'vendor/LICENSE').write_text('Test license\n')
    assets=app/'assets/script-libraries';(assets/'README.keep').write_text('unrelated')
    def snapshot():
        return {str(path.relative_to(app)):(path.read_bytes(),path.stat().st_mtime_ns)
                for path in [*(assets.iterdir()),app/'browser_script_libraries.hpp'] if path.is_file()}
    def run(value=manifest, success=True):
        (app/'runtime/libraries.json').write_text(json.dumps(value))
        result=subprocess.run([sys.executable,'-O',str(generator),str(compiler)],capture_output=True,text=True)
        assert (result.returncode==0)==success,(result.stdout,result.stderr)
        if not success:assert 'script libraries:' in result.stderr,result.stderr
    run();old=hashlib.sha256((app/'vendor/example.js').read_bytes()).hexdigest()
    assert (assets/(old+'.qjb')).is_file()
    before=snapshot();run();assert snapshot()==before,'no-op regeneration rewrote output'
    changed=copy.deepcopy(manifest)
    changed['libraries'].append({'id':'second','source':'vendor/second.js','license':'vendor/LICENSE','filename':'second.js'})
    for raw in (b'FAIL',b'BAD',b'LARGE'):
        (app/'vendor/second.js').write_bytes(raw);run(changed,False)
        assert snapshot()==before,'failed build published partial output'
    invalid=copy.deepcopy(manifest);invalid['libraries'].append(copy.deepcopy(invalid['libraries'][0]));run(invalid,False)
    invalid['libraries'][-1]['id']='second';run(invalid,False) # ambiguous duplicate source identity
    for key,value in [('id','../escape'),('source','../outside.js'),('filename','bad\0name')]:
        invalid=copy.deepcopy(manifest);invalid['libraries'][0][key]=value;run(invalid,False)
    for raw in (b'',b'x'*32769):
        (app/'vendor/second.js').write_bytes(raw);run(changed,False)
    assert snapshot()==before
    (app/'vendor/example.js').write_bytes(b'function example(){return 2}')
    run();new=hashlib.sha256((app/'vendor/example.js').read_bytes()).hexdigest()
    assert not (assets/(old+'.qjb')).exists() and (assets/(new+'.qjb')).is_file()
    run({'format':manifest['format'],'libraries':[]})
    assert set(path.name for path in assets.iterdir())=={'README.keep'}
    assert 'array<LibraryEntry,0>' in (app/'browser_script_libraries.hpp').read_text()
print('library generator: no-op/atomic failure/identity/change/stale/empty/path/bounds PASS')
