#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
"""Compare original scan-window lake DFS including all writes and root frame."""
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
spec=importlib.util.spec_from_file_location('architect_generator',compat/'generate_architect.py');g=importlib.util.module_from_spec(spec);spec.loader.exec_module(g);generated=a.output/'pxa_architect.c';g.generate(source,generated);replacement=(compat/'fill_lake_stack.c.inc').read_text();restored=generated.read_text()
for original,filename,_ in g.pinned_adaptations():restored=restored.replace((compat/filename).read_text(),original,1)
assert restored==source.read_text()
negative=[]
for name,text,override in [('changed-source',source.read_text()+'\n',False),('duplicate-fill-lake',source.read_text()+g.FILL_LAKE_ORIGINAL_FUNCTION,True)]:
 bad=a.output/(name+'.c');bad.write_text(text)
 if override:g.EXPECTED_SOURCE_SHA256=hashlib.sha256(bad.read_bytes()).hexdigest()
 output=a.output/(name+'-output.c')
 try:g.generate(bad,output)
 except SystemExit as e:assert not output.exists();negative.append({'case':name,'rejected':True,'error':str(e)})
 else:raise AssertionError('Generator accepted '+name)
original=g.FILL_LAKE_ORIGINAL_FUNCTION.replace('fillLake','oracle_fillLake').replace('{\n','{\n    oracle_enter(x,y);\n',1);last=original.rfind('}');original=original[:last]+'    --oracle_depth;\n'+original[last:]
adapter=replacement[replacement.index('static void fillLake('):].replace('fillLake','adapter_fillLake').replace('    unsigned frame_count = 1;','    unsigned frame_count = 1;\n    record_event(0,x,y,0);\n    observe_frames(frame_count);',1).replace('            ++frame_count;','            ++frame_count;\n            record_event(0,i,j,0);\n            observe_frames(frame_count);',1)
for assignment,event in [('unfilledLakeMap[i][j] = false;','record_event(1,i,j,0);'),('pmap[i][j].layers[LIQUID] = liquid;','record_event(2,i,j,liquid);'),('wreathMap[i][j] = 1;','record_event(3,i,j,1);')]:
 assert original.count(assignment)==adapter.count(assignment)==1
 original=original.replace(assignment,assignment+event,1);adapter=adapter.replace(assignment,assignment+event,1)
prefix=r"""
void pxa_brogue_cooperate(void) {}
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define DCOLS 79
#define DROWS 29
#define CELLS (DCOLS*DROWS)
#define CAP (CELLS+1)
#define EVENTS (4*CELLS+1)
#define CHECK(c) do{if(!(c)){fprintf(stderr,"check failed line=%d case=%u\n",__LINE__,cases);exit(2);}}while(0)
#define brogueAssert(c) CHECK(c)
#define coordinatesAreInMap(x,y) ((x)>=0 && (x)<DCOLS && (y)>=0 && (y)<DROWS)
enum {DUNGEON,LIQUID,GAS,SURFACE};
struct pcell{short layers[4];};
static struct pcell (*pmap)[DROWS];
static unsigned cases,which,counts[2],oracle_depth,oracle_peak,adapter_peak;
static short trace[2][EVENTS][4];
static uint64_t random_state=UINT64_C(0x174a9bfed7);
static uint32_t random32(void){random_state^=random_state<<13;random_state^=random_state>>7;random_state^=random_state<<17;return(uint32_t)random_state;}
static void record_event(short kind,short x,short y,short value){CHECK(counts[which]<EVENTS);short *r=trace[which][counts[which]++];r[0]=kind;r[1]=x;r[2]=y;r[3]=value;}
static void oracle_enter(short x,short y){++oracle_depth;if(oracle_depth>oracle_peak)oracle_peak=oracle_depth;record_event(0,x,y,0);}
static void observe_frames(unsigned n){CHECK(n<=CAP);if(n>adapter_peak)adapter_peak=n;}
struct short_grid{uint64_t pre;short cells[DCOLS][DROWS];uint64_t post;};
struct char_grid{uint64_t pre;char cells[DCOLS][DROWS];uint64_t post;};
struct pmap_grid{uint64_t pre;struct pcell cells[DCOLS][DROWS];uint64_t post;};
"""
suffix=r"""
static void run_case(short initial[DCOLS][DROWS],short sx,short sy,short liquid,short scan){
 struct short_grid a={.pre=0xab89,.post=0x914d},b={.pre=0xab89,.post=0x914d};struct char_grid wa={.pre=0x118c,.post=0xab17},wb={.pre=0x118c,.post=0xab17};struct pmap_grid pa={.pre=0x961f,.post=0x718a},pb={.pre=0x961f,.post=0x718a};short *ar[DCOLS],*br[DCOLS];
 memcpy(a.cells,initial,sizeof(a.cells));memcpy(b.cells,initial,sizeof(b.cells));
 for(int x=0;x<DCOLS;++x){ar[x]=a.cells[x];br[x]=b.cells[x];for(int y=0;y<DROWS;++y){wa.cells[x][y]=wb.cells[x][y]=(x+y)%5;for(int l=0;l<4;++l)pa.cells[x][y].layers[l]=pb.cells[x][y].layers[l]=(x*3+y+l)%23;}}
 counts[0]=counts[1]=0;oracle_depth=0;which=0;pmap=pa.cells;oracle_fillLake(sx,sy,liquid,scan,wa.cells,ar);CHECK(!oracle_depth);
 which=1;pmap=pb.cells;adapter_fillLake(sx,sy,liquid,scan,wb.cells,br);
 CHECK(a.pre==0xab89 && b.pre==0xab89 && a.post==0x914d && b.post==0x914d);CHECK(wa.pre==0x118c && wb.pre==0x118c && wa.post==0xab17 && wb.post==0xab17);CHECK(pa.pre==0x961f && pb.pre==0x961f && pa.post==0x718a && pb.post==0x718a);
 CHECK(!memcmp(a.cells,b.cells,sizeof(a.cells)));CHECK(!memcmp(wa.cells,wb.cells,sizeof(wa.cells)));CHECK(!memcmp(pa.cells,pb.cells,sizeof(pa.cells)));CHECK(counts[0]==counts[1]);CHECK(!memcmp(trace[0],trace[1],counts[0]*sizeof(trace[0][0])));++cases;
}
int main(void){static short initial[DCOLS][DROWS];
 for(int x=0;x<DCOLS;++x)for(int y=0;y<DROWS;++y)initial[x][y]=1;
 run_case(initial,0,0,3,1);CHECK(counts[0]==EVENTS && oracle_peak==CAP && adapter_peak==CAP);
 run_case(initial,DCOLS-1,DROWS-1,7,4);CHECK(counts[0]==EVENTS);
 run_case(initial,DCOLS/2,DROWS/2,11,0);CHECK(counts[0]==5);
 run_case(initial,DCOLS/2,DROWS/2,11,-1);CHECK(counts[0]==1);
 memset(initial,0,sizeof(initial));initial[0][0]=initial[DCOLS-1][DROWS-1]=1;run_case(initial,0,0,2,4);CHECK(counts[0]==5);
 for(int x=0;x<DCOLS;++x)for(int y=0;y<DROWS;++y)initial[x][y]=(y%2==0 || ((y/2)%2==0?x==DCOLS-1:x==0));run_case(initial,0,0,3,1);
 for(unsigned n=0;n<512;++n){unsigned density=n%101;for(int x=0;x<DCOLS;++x)for(int y=0;y<DROWS;++y)initial[x][y]=random32()%100<density?(n%2?-2:1):0;short widths[]={0,1,2,4,8};short scan=widths[n%5];run_case(initial,random32()%DCOLS,random32()%DROWS,2+n%11,scan);}
 printf("{\"cases\":%u,\"map_cells\":%u,\"initial_root_plus_cells_capacity\":%u,\"oracle_max_depth\":%u,\"adapter_max_frames\":%u,\"unfilled_pmap_all_layers_wreath_equal\":true,\"complete_call_and_three_write_order_equal\":true}\n",cases,CELLS,CAP,oracle_peak,adapter_peak);return 0;
}
"""
harness=a.output/'fill-lake-stack-equivalence.c';harness.write_text(prefix+original+'\n'+adapter+'\n'+suffix);command=[a.cc,'-std=c11','-O2','-g','-fno-optimize-sibling-calls','-fsanitize=address,undefined','-fno-omit-frame-pointer',str(harness),'-o',str(a.output/'fill-lake-stack-equivalence')]
with(a.output/'compile.log').open('w')as log:subprocess.run(command,stdout=log,stderr=subprocess.STDOUT,check=True)
r=subprocess.run([str(a.output/'fill-lake-stack-equivalence')],stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=1:abort_on_error=1','UBSAN_OPTIONS':'halt_on_error=1:print_stacktrace=1'});(a.output/'run.stdout').write_text(r.stdout);(a.output/'run.stderr').write_text(r.stderr);assert r.returncode==0,(r.returncode,r.stderr[-3000:]);result={'passed':True,'native_auxiliary_test_only':True,'compiler_command':command,'upstream_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),'generated_round_trip_exact':True,'fail_closed_checks':negative,'rng_scope':'No RNG calls in helper; liquidType RNG remains in unchanged caller before helper. Tests compare all state changes/call order.',**json.loads(r.stdout)};(a.output/'result.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))
