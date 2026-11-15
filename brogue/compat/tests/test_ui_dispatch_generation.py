#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
"""Pinned UI generation, exact roundtrip, and both playback build modes."""
from pathlib import Path
import argparse,copy,hashlib,importlib.util,json,os,subprocess,tempfile

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--cc',default=os.environ.get('CC','clang'))
    parser.add_argument('--target')
    args=parser.parse_args();out=args.output.resolve();out.mkdir(parents=True,exist_ok=False)
    app=Path(__file__).resolve().parents[2];compat=app/'compat';vendor=app/'vendor/src/brogue'
    spec=importlib.util.spec_from_file_location('ui_generator',compat/'generate_ui.py')
    generator=importlib.util.module_from_spec(spec);spec.loader.exec_module(generator)
    result={'passed':False,'generator_sha256':hashlib.sha256((compat/'generate_ui.py').read_bytes()).hexdigest(),'syntax':[],'negative_cases':[],'units':[]}
    try:
        for name,pin in generator.PINS.items():
            source=vendor/name;generated=out/('pxa_'+name)
            generator.generate(source,generated);original=source.read_text();helper=(compat/pin['helper']).read_text()
            assert original.count(pin['function'])==1
            assert generated.read_text().replace(helper,pin['function'],1)==original
            assert original.split(pin['function'],1)==generated.read_text().split(helper,1)
            result['units'].append({'unit':name,'original_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),'generated_sha256':hashlib.sha256(generated.read_bytes()).hexdigest(),'inverse_exact':True})
            for playback in [False,True]:
                for release in [False,True]:
                    label=name+f'-playback{int(playback)}-release{int(release)}'
                    argv=[args.cc,'-fsyntax-only','-O3','-Wno-incompatible-pointer-types','-Wno-int-conversion','-DBROGUE_EXTRA_VERSION="+104a20f-pxa"','-DDATADIR=.']
                    if args.target:argv+=['--target='+args.target]
                    if playback:argv+=['-DENABLE_PLAYBACK_SWITCH=1']
                    if release:argv+=['-DNDEBUG']
                    argv += ['-I'+str(vendor),'-I'+str(app/'vendor/src/platform'),'-I'+str(app/'vendor/src/variants'),str(generated)]
                    done=subprocess.run(argv,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True)
                    log=out/(label+'.log');log.write_text(done.stdout)
                    result['syntax'].append({'unit':name,'playback':playback,'release':release,'exit_code':done.returncode,'argv':argv,'log':str(log)})
                    assert done.returncode==0,label
        with tempfile.TemporaryDirectory(prefix='negative-',dir=out) as temporary:
            root=Path(temporary);header=vendor/'Rogue.h';(root/'Rogue.h').write_bytes(header.read_bytes())
            source=root/'IO.c';original=(vendor/'IO.c').read_bytes();original_pins=copy.deepcopy(generator.PINS)
            def reject(label,alter):
                generator.PINS=copy.deepcopy(original_pins);source.write_bytes(original);(root/'Rogue.h').write_bytes(header.read_bytes());alter()
                try:generator.generate(source,root/'result.c')
                except SystemExit as error:result['negative_cases'].append({'case':label,'rejected':True,'reason':str(error)})
                else:raise AssertionError('Generator accepted '+label)
            reject('upstream byte change',lambda:source.write_bytes(original+b'\n'))
            reject('header macro byte change',lambda:(root/'Rogue.h').write_bytes(header.read_bytes()+b'\n'))
            def duplicate():
                source.write_bytes(original+generator.PINS['IO.c']['function'].encode());generator.PINS['IO.c']['sha256']=hashlib.sha256(source.read_bytes()).hexdigest()
            reject('duplicate pinned function after independently updated whole-file pin',duplicate)
            def missing():
                source.write_bytes(original.replace(generator.PINS['IO.c']['function'].encode(),b'',1));generator.PINS['IO.c']['sha256']=hashlib.sha256(source.read_bytes()).hexdigest()
            reject('missing pinned function after independently updated whole-file pin',missing)
            # Unknown-unit rejection is exercised separately, without swallowing
            # unexpected exceptions from fixture setup.
            generator.PINS=copy.deepcopy(original_pins);source.write_bytes(original);unexpected=root/'Unexpected.c';unexpected.write_bytes(original)
            try:generator.generate(unexpected,root/'result.c')
            except SystemExit as error:result['negative_cases'].append({'case':'unknown unit','rejected':True,'reason':str(error)})
            else:raise AssertionError('Generator accepted unknown unit')
        result['passed']=True
    finally:
        (out/'result.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps({'passed':result['passed'],'syntax_configurations':len(result['syntax']),'negative_cases':len(result['negative_cases']),'result':str(out/'result.json')}))

if __name__=='__main__':main()
