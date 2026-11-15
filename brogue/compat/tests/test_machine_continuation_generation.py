#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
"""Check pinned machine-continuation integration and fail-closed generation."""
from pathlib import Path
import sys
sys.dont_write_bytecode=True
import argparse,hashlib,importlib.util,json,shutil
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output',type=Path,required=True);a=p.parse_args();a.output.mkdir(parents=True,exist_ok=False)
compat=Path(__file__).resolve().parents[1];app=compat.parent;source=app/'vendor/src/brogue/Architect.c'
spec=importlib.util.spec_from_file_location('machine_architect_generator',compat/'generate_architect.py');g=importlib.util.module_from_spec(spec);spec.loader.exec_module(g)
raw=source.read_text();generated=a.output/'pxa_architect.c';g.generate(source,generated);restored=generated.read_text()
for original,filename,_ in g.pinned_adaptations():restored=restored.replace((compat/filename).read_text(),original,1)
assert restored==raw
assert hashlib.sha256(g.MACHINE_BUILD_ORIGINAL_FUNCTION.encode()).hexdigest()==g.MACHINE_BUILD_EXPECTED_SHA256
assert len(g.MACHINE_BUILD_RECURSIVE_SITES)==2
assert all(raw.count(site)==1 for site in g.MACHINE_BUILD_RECURSIVE_SITES)
adapter=(compat/'build_machine_stack.c.inc').read_text();header=raw[:raw.index('*/')+2];assert adapter.startswith(header)
assert adapter.count('boolean buildAMachine(')==1
assert adapter.count('return PXA_MACHINE_CHILD;')==2
for number in (1,2):
 assert adapter.count('goto pxa_machine_resume_'+str(number)+';')==1
 assert adapter.count('pxa_machine_resume_'+str(number)+':')==1
 assert adapter.count('frame->resume = '+str(number)+';')==1
assert adapter.count('return PXA_MACHINE_COMPLETE;')==9
assert adapter.count('pxa_machine_free_data(frame,')==9
assert adapter.count('pxa_machine_free_grid(frame,')==9
assert adapter.count('pxa_machine_allocate_grid(frame)')==2
FIELDS = ['a_bp', 'a_originX', 'a_originY', 'a_requiredMachineFlags', 'a_adoptiveItem', 'a_parentSpawnedItems', 'a_parentSpawnedMonsters', 'l00_totalFreq', 'l01_instance', 'l02_instanceCount', 'l03_itemCount', 'l04_monsterCount', 'l05_qualifyingTileCount', 'l06_distanceMap', 'l07_personalSpace', 'l08_locationFailsafe', 'l09_machineNumber', 'l10_alternativeFlags', 'l11_DFSucceeded', 'l12_terrainSucceeded', 'l13_generateEverywhere', 'l14_tryAgain', 'l15_success', 'l16_skipFeature', 'l17_monst', 'l18_torchBearer', 'l19_leader', 'l20_theItem', 'l21_torch', 'l22_feature', 'l23_p', 'l24_chooseBP', 'l25_chooseLocation', 'l26_failsafe', 'l27_i', 'l28_randIndex', 'l29_i', 'l30_i', 'l31_j', 'l32_randIndex', 'l33_originLoc', 'l34_k', 'l35_i', 'l36_j', 'l37_i', 'l38_j', 'l39_layer', 'l40_i', 'l41_i', 'l42_j', 'l43_distance25', 'l44_distance75', 'l45_i', 'l46_i', 'l47_i', 'l48_j', 'l49_i', 'l50_randIndex', 'l51_i', 'l52_feat', 'l53_distanceBound', 'l54_i', 'l55_j', 'l56_featX', 'l57_featY', 'l58_randIndex', 'l59_i', 'l60_j', 'l61_i', 'l62_j', 'l63_i', 'l64_j', 'l65_j', 'l66_it', 'l67_monst', 'l68_i', 'l69_j', 'l70_i', 'l71_i']
assert len(FIELDS)==79 and len(set(FIELDS))==79
for field in FIELDS:assert 'frame->'+field in adapter
negative=[]
for name,text in [('changed-source',raw+'\n'),('duplicate-machine',raw+g.MACHINE_BUILD_ORIGINAL_FUNCTION),('duplicate-adopt-site',raw+'\n/* '+g.MACHINE_BUILD_RECURSIVE_SITES[0]+' */\n'),('duplicate-vestibule-site',raw+'\n/* '+g.MACHINE_BUILD_RECURSIVE_SITES[1]+' */\n')]:
 f=a.output/(name+'.c');f.write_text(text);g.EXPECTED_SOURCE_SHA256=hashlib.sha256(f.read_bytes()).hexdigest() if name!='changed-source' else '0'*64;out=a.output/(name+'-output.c')
 try:g.generate(f,out)
 except SystemExit as error:assert not out.exists();negative.append({'case':name,'rejected_before_output':True,'reason':str(error)})
 else:raise AssertionError('Generator accepted '+name)
g.EXPECTED_SOURCE_SHA256=hashlib.sha256(source.read_bytes()).hexdigest()
staged=a.output/'bad-template';shutil.copytree(compat,staged);template=staged/'build_machine_stack.c.inc';template.write_text(template.read_text()+'\nboolean buildAMachine(\n');g.__file__=str(staged/'generate_architect.py');out=a.output/'bad-template-output.c'
try:g.generate(source,out)
except SystemExit as error:assert not out.exists();negative.append({'case':'duplicate-adapter-public-entry','rejected_before_output':True,'reason':str(error)})
else:raise AssertionError('Generator accepted duplicate public entry')
result={'passed':True,'source_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),'adapter_sha256':hashlib.sha256((compat/'build_machine_stack.c.inc').read_bytes()).hexdigest(),'whole_generated_TU_roundtrip_exact':True,'local_fields':72,'parameter_fields':7,'direct_child_continuations':2,'original_return_paths':9,'license_notice_retained':True,'negative_tests':negative,'scope':'Generation structure only; whole-engine A/B and real SDK gameplay remain required.'};(a.output/'result.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result))
