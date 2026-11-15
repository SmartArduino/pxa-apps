#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
"""Compare original loop-pruning recursion, exact flags, return and call order."""
from pathlib import Path
import sys
sys.dont_write_bytecode = True
import argparse,hashlib,importlib.util,json,os,re,subprocess
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output',type=Path,required=True);p.add_argument('--cc',default='cc');a=p.parse_args();a.output.mkdir(parents=True,exist_ok=False)
compat=Path(__file__).resolve().parents[1];app=compat.parent;source=app/'vendor/src/brogue/Architect.c';rogue=(app/'vendor/src/brogue/Rogue.h').read_text()
for name,value in [('COLS','100'),('MESSAGE_LINES','3'),('STAT_BAR_WIDTH','20')]:assert re.search(r'^#define\s+'+name+r'\s+'+value+r'\s*(?://[^\n]*)?$',rogue,re.M)
assert '#define ROWS                    (31 + MESSAGE_LINES)' in rogue
assert '#define DCOLS                   (COLS - STAT_BAR_WIDTH - 1)' in rogue
assert '#define DROWS                   (ROWS - MESSAGE_LINES - 2)' in rogue
assert 'DIRECTION_COUNT = 8,' in rogue
assert 'const short cDirs[8][2] = {{0, 1}, {1, 1}, {1, 0}, {1, -1}, {0, -1}, {-1, -1}, {-1, 0}, {-1, 1}};' in (app/'vendor/src/brogue/GlobalsBase.c').read_text()
spec=importlib.util.spec_from_file_location('architect_generator',compat/'generate_architect.py');g=importlib.util.module_from_spec(spec);spec.loader.exec_module(g);generated=a.output/'pxa_architect.c';g.generate(source,generated);replacement=(compat/'check_loopiness_stack.c.inc').read_text();restored=generated.read_text()
for original,filename,_ in g.pinned_adaptations():restored=restored.replace((compat/filename).read_text(),original,1)
assert restored==source.read_text()
negative=[]
for name,text,override in [('changed-source',source.read_text()+'\n',False),('duplicate-loopiness',source.read_text()+g.LOOPINESS_ORIGINAL_FUNCTION,True)]:
 bad=a.output/(name+'.c');bad.write_text(text)
 if override:g.EXPECTED_SOURCE_SHA256=hashlib.sha256(bad.read_bytes()).hexdigest()
 output=a.output/(name+'-output.c')
 try:g.generate(bad,output)
 except SystemExit as e:assert not output.exists();negative.append({'case':name,'rejected':True,'error':str(e)})
 else:raise AssertionError('Generator accepted '+name)
original=g.LOOPINESS_ORIGINAL_FUNCTION.replace('checkLoopiness','oracle_checkLoopiness').replace('static boolean oracle_checkLoopiness(','static boolean oracle_checkLoopiness_impl(',1)
adapter=replacement[replacement.index('static boolean pxa_remove_loopiness('):];adapter=adapter.replace('{\n','{\n    record_event(0,x,y,0);\n',1);adapter=adapter.replace('    frames[0] =','    observe_frames(frame_count);\n    frames[0] =',1).replace('            ++frame_count;','            ++frame_count;\n            observe_frames(frame_count);',1)
statement='pmap[x][y].flags &= ~IN_LOOP;'
assert original.count(statement)==adapter.count(statement)==1
original=original.replace(statement,statement+'record_event(1,x,y,pmap[x][y].flags);',1);adapter=adapter.replace(statement,statement+'record_event(1,x,y,pmap[x][y].flags);',1)
prefix=r"""
void pxa_brogue_cooperate(void) {}
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define DCOLS 79
#define DROWS 29
#define CAP (DCOLS*DROWS)
#define EVENTS (9*CAP+1)
#define DIRECTION_COUNT 8
#define IN_LOOP UINT32_C(0x10)
#define CHECK(c) do{if(!(c)){fprintf(stderr,"check failed line=%d case=%u\n",__LINE__,cases);exit(2);}}while(0)
#define brogueAssert(c) CHECK(c)
#define coordinatesAreInMap(x,y) ((x)>=0 && (x)<DCOLS && (y)>=0 && (y)<DROWS)
typedef bool boolean;
static const short cDirs[8][2]={{0,1},{1,1},{1,0},{1,-1},{0,-1},{-1,-1},{-1,0},{-1,1}};
struct pcell{uint32_t flags;};
static struct pcell (*pmap)[DROWS];
struct grid{uint64_t pre;struct pcell cells[DCOLS][DROWS];uint64_t post;};
struct event{uint32_t kind,x,y,value;};
static unsigned cases,which,counts[2],oracle_depth,oracle_peak,adapter_peak;
static struct event trace[2][EVENTS];
static uint64_t random_state=UINT64_C(0x174bc6ad758);
static uint32_t random32(void){random_state^=random_state<<13;random_state^=random_state>>7;random_state^=random_state<<17;return(uint32_t)random_state;}
static void record_event(uint32_t kind,short x,short y,uint32_t value){CHECK(counts[which]<EVENTS);trace[which][counts[which]++]=(struct event){kind,(uint32_t)x,(uint32_t)y,value};}
static void observe_frames(unsigned n){CHECK(n<=CAP);if(n>adapter_peak)adapter_peak=n;}
static boolean oracle_checkLoopiness(short x,short y);
"""
wrapper=r"""
static boolean oracle_checkLoopiness(short x,short y){
 ++oracle_depth;CHECK(oracle_depth<=CAP+1);if(oracle_depth>oracle_peak)oracle_peak=oracle_depth;record_event(0,x,y,0);
 boolean result=oracle_checkLoopiness_impl(x,y);--oracle_depth;return result;
}
"""
suffix=r"""
static void run_case(struct pcell initial[DCOLS][DROWS],short sx,short sy){
 struct grid a={.pre=0x7138,.post=0x948a},b={.pre=0x7138,.post=0x948a};memcpy(a.cells,initial,sizeof(a.cells));memcpy(b.cells,initial,sizeof(b.cells));counts[0]=counts[1]=0;oracle_depth=0;
 which=0;pmap=a.cells;boolean expected=oracle_checkLoopiness(sx,sy);CHECK(!oracle_depth);
 which=1;pmap=b.cells;boolean actual=checkLoopiness(sx,sy);
 CHECK(a.pre==0x7138 && b.pre==0x7138 && a.post==0x948a && b.post==0x948a);CHECK(expected==actual);CHECK(!memcmp(a.cells,b.cells,sizeof(a.cells)));CHECK(counts[0]==counts[1]);CHECK(!memcmp(trace[0],trace[1],counts[0]*sizeof(trace[0][0])));++cases;
}
int main(void){static struct pcell initial[DCOLS][DROWS];
 for(int x=0;x<DCOLS;++x)for(int y=0;y<DROWS;++y)initial[x][y].flags=IN_LOOP|UINT32_C(0x82000401);
 run_case(initial,0,0);run_case(initial,DCOLS/2,DROWS/2);run_case(initial,DCOLS-1,DROWS-1);
 memset(initial,0,sizeof(initial));for(int x=0;x<DCOLS;++x)initial[x][2].flags=IN_LOOP|4;run_case(initial,0,2);CHECK(oracle_peak>=DCOLS);
 memset(initial,0,sizeof(initial));for(int y=1;y<DROWS-1;++y)for(int x=1;x<DCOLS-1;++x)if(y%3==1 || ((y/3)%2==0?x==DCOLS-2:x==1))initial[x][y].flags=IN_LOOP|2;run_case(initial,1,1);
 for(unsigned mask=0;mask<512;++mask){
  memset(initial,0,sizeof(initial));for(unsigned i=0;i<9;++i)initial[DCOLS/2+(int)(i%3)-1][DROWS/2+(int)(i/3)-1].flags=(mask&(1u<<i)?IN_LOOP:0)|UINT32_C(0x94123);
  for(unsigned i=0;i<9;++i)run_case(initial,DCOLS/2+(int)(i%3)-1,DROWS/2+(int)(i/3)-1);
 }
 for(unsigned n=0;n<1024;++n){unsigned density=n%101;for(int x=0;x<DCOLS;++x)for(int y=0;y<DROWS;++y)initial[x][y].flags=(random32()&~IN_LOOP)|(random32()%100<density?IN_LOOP:0);short x=random32()%DCOLS,y=random32()%DROWS;run_case(initial,x,y);if(n%32==0){run_case(initial,0,0);run_case(initial,DCOLS-1,DROWS-1);}}
 printf("{\"cases\":%u,\"map_cells_changed_frame_capacity\":%u,\"oracle_max_call_depth\":%u,\"adapter_max_mutated_frames\":%u,\"full_flagmap_root_return_and_call_write_sequence_equal\":true,\"exhaustive_3x3_masks\":512}\n",cases,CAP,oracle_peak,adapter_peak);return 0;
}
"""
harness=a.output/'loopiness-stack-equivalence.c';harness.write_text(prefix+original+'\n'+wrapper+'\n'+adapter+'\n'+suffix);command=[a.cc,'-std=c11','-O2','-g','-fno-optimize-sibling-calls','-fsanitize=address,undefined','-fno-omit-frame-pointer',str(harness),'-o',str(a.output/'loopiness-stack-equivalence')]
with(a.output/'compile.log').open('w')as log:subprocess.run(command,stdout=log,stderr=subprocess.STDOUT,check=True)
r=subprocess.run([str(a.output/'loopiness-stack-equivalence')],stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=1:abort_on_error=1','UBSAN_OPTIONS':'halt_on_error=1:print_stacktrace=1'});(a.output/'run.stdout').write_text(r.stdout);(a.output/'run.stderr').write_text(r.stderr);assert r.returncode==0,(r.returncode,r.stderr[-3000:]);result={'passed':True,'native_auxiliary_test_only':True,'compiler_command':command,'upstream_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),'generated_round_trip_exact':True,'fail_closed_checks':negative,'flag_fixture_scope':'Original and adapter operate the same abstract IN_LOOP flag bit, preserving every other bit; actual game layout is checked by original SDK acceptance.',**json.loads(r.stdout)};(a.output/'result.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))
