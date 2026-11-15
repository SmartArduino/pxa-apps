#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
"""Compare the actual recursive machine traversal including failed ancestors."""
from pathlib import Path
import sys
sys.dont_write_bytecode=True
import argparse,hashlib,importlib.util,json,os,subprocess
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output',type=Path,required=True);p.add_argument('--cc',default='cc');a=p.parse_args();a.output.mkdir(parents=True,exist_ok=False);compat=Path(__file__).resolve().parents[1];app=compat.parent;source=app/'vendor/src/brogue/Architect.c'
spec=importlib.util.spec_from_file_location('architect_generator',compat/'generate_architect.py');g=importlib.util.module_from_spec(spec);spec.loader.exec_module(g);generated=a.output/'pxa_architect.c';g.generate(source,generated);restored=generated.read_text()
for original,filename,_ in g.pinned_adaptations():restored=restored.replace((compat/filename).read_text(),original,1)
assert restored==source.read_text();negative=[]
for name,text,override in [('changed-source',source.read_text()+'\n',False),('duplicate-machine',source.read_text()+g.MACHINE_ORIGINAL_FUNCTION,True)]:
 bad=a.output/(name+'.c');bad.write_text(text)
 if override:g.EXPECTED_SOURCE_SHA256=hashlib.sha256(bad.read_bytes()).hexdigest()
 output=a.output/(name+'-output.c')
 try:g.generate(bad,output)
 except SystemExit as error:assert not output.exists();negative.append({'case':name,'rejected':True,'error':str(error)})
 else:raise AssertionError('Generator accepted '+name)
oracle=g.MACHINE_ORIGINAL_FUNCTION.replace('addTileToMachineInteriorAndIterate','oracle_machine').replace('static boolean oracle_machine(','static boolean oracle_machine_impl(',1).replace('interior[startX][startY] = true;','interior[startX][startY] = true;record_event(1,startX,startY,1);',1)
adapter=(compat/'machine_interior_stack.c.inc').read_text();adapter=adapter[adapter.index('static boolean addTileToMachineInteriorAndIterate('):].replace('    interior[startX][startY] = true;','    record_event(0,startX,startY,0);\n    interior[startX][startY] = true;record_event(1,startX,startY,1);',1).replace('                interior[newX][newY] = true;','                record_event(0,newX,newY,0);\n                interior[newX][newY] = true;record_event(1,newX,newY,1);',1).replace('            --frame_count;','            record_event(2,frame->x,frame->y,good);\n            --frame_count;',1).replace('    frames[0] =','    observe_frames(frame_count);\n    frames[0] =',1).replace('                ++frame_count;','                ++frame_count;observe_frames(frame_count);',1)
oracle=oracle.replace('pmap[newX][newY].flags','flags_at(newX,newY)');adapter=adapter.replace('pmap[newX][newY].flags','flags_at(newX,newY)')
prefix=r"""
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define DCOLS 79
#define DROWS 29
#define CAP (DCOLS*DROWS)
#define EVENTS (20*CAP)
#define CHECK(c) do{if(!(c)){fprintf(stderr,"line=%d case=%u\n",__LINE__,cases);exit(2);}}while(0)
#define brogueAssert(c) CHECK(c)
#define coordinatesAreInMap(x,y) ((x)>=0 && (x)<DCOLS && (y)>=0 && (y)<DROWS)
#define HAS_ITEM 1u
#define IS_IN_MACHINE 2u
#define IS_GATE_SITE 4u
typedef bool boolean;
static void pxa_brogue_cooperate(void) {}
static const short nbDirs[4][2]={{0,-1},{0,1},{-1,0},{1,0}};
static uint32_t(*flags_map)[DROWS];static short(*chokeMap)[DROWS];
struct char_grid{uint64_t pre;char cells[DCOLS][DROWS];uint64_t post;};
struct event{int kind,x,y,value;};static struct event trace[2][EVENTS];static unsigned counts[2],which,cases,oracle_depth,oracle_peak,adapter_peak,failed_cases;
static uint64_t random_state=UINT64_C(0x759a1d3e);
static uint32_t random32(void){random_state^=random_state<<13;random_state^=random_state>>7;random_state^=random_state<<17;return(uint32_t)random_state;}
static void record_event(int kind,short x,short y,int value){CHECK(counts[which]<EVENTS);trace[which][counts[which]++]=(struct event){kind,x,y,value};}
static void observe_frames(unsigned n){CHECK(n<=CAP);if(n>adapter_peak)adapter_peak=n;}
static uint32_t flags_at(short x,short y){CHECK(coordinatesAreInMap(x,y));record_event(3,x,y,flags_map[x][y]);return flags_map[x][y];}
static boolean oracle_machine(char[DCOLS][DROWS],short,short);
"""
wrapper=r"""
static boolean oracle_machine(char grid[DCOLS][DROWS],short x,short y){++oracle_depth;CHECK(oracle_depth<=CAP);if(oracle_depth>oracle_peak)oracle_peak=oracle_depth;record_event(0,x,y,0);boolean n=oracle_machine_impl(grid,x,y);record_event(2,x,y,n);--oracle_depth;return n;}
"""
suffix=r"""
static void run(char initial[DCOLS][DROWS],uint32_t flags[DCOLS][DROWS],short choke[DCOLS][DROWS],short x,short y){struct char_grid a={.pre=0x1abc,.post=0x789a},b={.pre=0x1abc,.post=0x789a};memcpy(a.cells,initial,sizeof(a.cells));memcpy(b.cells,initial,sizeof(b.cells));uint32_t before_flags[DCOLS][DROWS];short before_choke[DCOLS][DROWS];memcpy(before_flags,flags,sizeof(before_flags));memcpy(before_choke,choke,sizeof(before_choke));flags_map=flags;chokeMap=choke;counts[0]=counts[1]=oracle_depth=0;which=0;boolean expected=oracle_machine(a.cells,x,y);CHECK(!oracle_depth);which=1;boolean actual=addTileToMachineInteriorAndIterate(b.cells,x,y);CHECK(expected==actual);if(!actual)++failed_cases;CHECK(a.pre==0x1abc && a.post==0x789a && b.pre==0x1abc && b.post==0x789a);CHECK(!memcmp(a.cells,b.cells,sizeof(a.cells)));CHECK(!memcmp(before_flags,flags,sizeof(before_flags)));CHECK(!memcmp(before_choke,choke,sizeof(before_choke)));CHECK(counts[0]==counts[1]);CHECK(!memcmp(trace[0],trace[1],counts[0]*sizeof(trace[0][0])));++cases;}
int main(void){static char initial[DCOLS][DROWS];static uint32_t flags[DCOLS][DROWS];static short choke[DCOLS][DROWS];run(initial,flags,choke,0,0);CHECK(oracle_peak==CAP && adapter_peak==CAP);
for(unsigned flag=1;flag<8;++flag){flags[DCOLS-1][DROWS-1]=flag;run(initial,flags,choke,0,0);}memset(flags,0,sizeof(flags));
for(short x=0;x<DCOLS;++x)for(short y=0;y<DROWS;++y){choke[x][y]=30000;initial[x][y]=0;}for(short x=0;x<DCOLS;++x)for(short y=0;y<DROWS;++y)if(x%2==0 || (x%4==1 && y==DROWS-1)||(x%4==3 && y==0))choke[x][y]=0;run(initial,flags,choke,0,0);run(initial,flags,choke,DCOLS-1,DROWS-1);
for(unsigned k=0;k<512;++k){for(short x=0;x<DCOLS;++x)for(short y=0;y<DROWS;++y){initial[x][y]=random32()%9==0?2:0;flags[x][y]=random32()%13==0?random32()%8:0;choke[x][y]=k%3==0?0:random32()%30001;}run(initial,flags,choke,random32()%DCOLS,random32()%DROWS);}CHECK(failed_cases>0);printf("{\"cases\":%u,\"capacity\":%d,\"oracle_max_depth\":%u,\"adapter_max_frames\":%u,\"false_return_cases\":%u,\"checks\":\"full interior grid, flag-query short-circuit, call/write/child-return order including failed ancestors, choke thresholds, readonly maps and canaries\"}\n",cases,CAP,oracle_peak,adapter_peak,failed_cases);return 0;}
"""
c=a.output/'oracle.c';c.write_text(prefix+oracle+wrapper+adapter+suffix);exe=a.output/'oracle';command=[a.cc,'-std=c11','-O2','-g','-fno-optimize-sibling-calls','-fsanitize=address,undefined','-fno-omit-frame-pointer',str(c),'-o',str(exe)]
r=subprocess.run(command,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True);(a.output/'compile.log').write_text(r.stdout);assert r.returncode==0,r.stdout
r=subprocess.run([str(exe)],stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=60,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1'));(a.output/'run.log').write_text(r.stdout);assert r.returncode==0,r.stdout
receipt={'passed':True,'source_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),'generated_roundtrip_exact':True,'generator_fail_closed':negative,'compile_command':command,'oracle':json.loads(r.stdout)};(a.output/'result.json').write_text(json.dumps(receipt,indent=2)+'\n');print(r.stdout.strip())
