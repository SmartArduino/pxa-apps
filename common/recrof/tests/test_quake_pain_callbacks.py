#!/usr/bin/env python3
"""Exercise the real pain dispatcher in Wasm, including the original ABI trap."""
import argparse
from pathlib import Path
import re
import shutil
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--output', type=Path, required=True)
parser.add_argument('--clang', type=Path)
args = parser.parse_args()
here = Path(__file__).resolve().parent
apps = here.parents[2]
quake = apps / 'quake'
root = apps.parents[1]
out = args.output.resolve()
out.mkdir(parents=True, exist_ok=True)
header = (quake/'vendor/quake/quakeProgs.h').read_text()
program = (quake/'vendor/quake/quakeProgs.c').read_text()
adapter = (quake/'port/callbacks.c').read_text()
registered = set(re.findall(r'set_qcc_th_pain\([^;]*?getFunctionIndex\((qcc_\w+)\)', program))
bridged = dict((fn, kind) for kind, fn in re.findall(r'PAIN_(NOARGS|ARGS)\((qcc_\w+)\)', adapter))
assert registered == set(bridged), (registered - set(bridged), set(bridged) - registered)
indices, prototypes, stubs, checks, lookup = [], [], [], [], []
for fn in sorted(registered):
    index = int(re.search(r'#define\s+'+fn+r'_func_index\s+(\d+)', header).group(1))
    result, params = re.search(r'^(\w+)\s+'+fn+r'\(([^)]*)\);', header, re.M).groups()
    params = re.sub(r'\s+', '', params)
    assert result == 'void' and params in ('void', 'int,float')
    assert bridged[fn] == ('NOARGS' if params == 'void' else 'ARGS')
    indices.append(f'#define {fn}_func_index {index}')
    lookup.append(f'case {index}:return (void *){fn};')
    prototypes.append(f'void {fn}({params});')
    signature = 'void' if params == 'void' else 'int attacker, float damage'
    body = '' if params == 'void' else 'last_attacker=attacker;last_damage=damage;'
    stubs.append(f'void {fn}({signature}){{last_function={index};++calls;{body}}}')
    condition = '' if params == 'void' else ' || last_attacker!=123 || last_damage!=79.5f'
    checks.append(f'pxa_quake_call_pain({index},123,79.5f);if(last_function!={index}{condition})return {index};')
(out/'quakedef.h').write_text('#pragma once\nvoid Host_Error(const char *,...);\n')
(out/'cprogs_def.h').write_text('#pragma once\n#define getFunctionIndex(fn) fn ## _func_index\n' + '\n'.join(indices+prototypes)+'\nvoid *getFunction(unsigned);\n')
(out/'harness.c').write_text('''#include "cprogs_def.h"
#include "callbacks.h"
static int last_function,last_attacker,calls,errors;
static float last_damage;
void Host_Error(const char *message,...){(void)message;++errors;}
'''+'\n'.join(stubs)+'\nvoid *getFunction(unsigned index){switch(index){'+''.join(lookup)+'default:return 0;}}\n'+'''
static void *volatile legacy_pain_target = (void *)qcc_player_pain;
void original_pain(void){((void (*)(int,float))legacy_pain_target)(123,79.5f);}
int verify_callbacks(void){
'''+'\n'.join(checks)+f'''
if(calls!={len(registered)})return -1;
pxa_quake_call_pain(65535,123,79.5f);
return errors==1?0:-2;
}}
''')
clang = args.clang or root/'deps/pxa-system/.pxa/wasi-sdk-34.0-x86_64-linux/bin/clang'
subprocess.run([str(clang), '-O1', '-std=c11', '-Wall', '-Wextra', '-Werror', '-nostdlib',
               '-I'+str(out), '-I'+str(quake/'port'), str(quake/'port/callbacks.c'), str(out/'harness.c'),
               '-Wl,--no-entry', '-Wl,--export=verify_callbacks', '-Wl,--export=original_pain',
               '-o', str(out/'callbacks.wasm')], check=True)
(out/'run.cjs').write_text('''const fs=require('node:fs');
(async()=>{
const {instance}=await WebAssembly.instantiate(fs.readFileSync(process.argv[2]),{});
const result=instance.exports.verify_callbacks();
if(result!==0)throw Error('callback verification failed: '+result);
let trapped=false;
try{instance.exports.original_pain();}catch(error){
if(!(error instanceof WebAssembly.RuntimeError))throw error;
trapped=true;console.log('Original mismatched callback reproduced: '+error.message);
}
if(!trapped)throw Error('The original mismatched callback must trap in Wasm');
console.log('All registered pain callbacks passed with their declared signatures');
})();
''')
subprocess.run([shutil.which('node') or 'node', str(out/'run.cjs'), str(out/'callbacks.wasm')], check=True)
print(f'Validated {len(registered)} actual Quake pain callback assignments')
