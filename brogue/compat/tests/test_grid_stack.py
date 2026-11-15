#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
"""Compare actual Brogue blob-fill recursion and bounded-frame adaptation."""
from pathlib import Path
import sys
sys.dont_write_bytecode = True
import argparse,hashlib,importlib.util,json,os,re,subprocess
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output',type=Path,required=True);p.add_argument('--cc',default='cc');a=p.parse_args();a.output.mkdir(parents=True,exist_ok=False)
compat=Path(__file__).resolve().parents[1];app=compat.parent;source=app/'vendor/src/brogue/Grid.c';rogue=(app/'vendor/src/brogue/Rogue.h').read_text()
for name,value in [('COLS','100'),('MESSAGE_LINES','3'),('STAT_BAR_WIDTH','20')]:assert re.search(r'^#define\s+'+name+r'\s+'+value+r'\s*(?://[^\n]*)?$',rogue,re.M)
assert '#define ROWS                    (31 + MESSAGE_LINES)' in rogue
assert '#define DCOLS                   (COLS - STAT_BAR_WIDTH - 1)' in rogue
assert '#define DROWS                   (ROWS - MESSAGE_LINES - 2)' in rogue
assert 'const short nbDirs[8][2] = {{0,-1}, {0,1}, {-1,0}, {1,0},' in (app/'vendor/src/brogue/GlobalsBase.c').read_text()
assert source.read_text().count('fillContiguousRegion(')==3
assert 'blobNumber = 2;' in source.read_text()
spec=importlib.util.spec_from_file_location('grid_generator',compat/'generate_grid.py');g=importlib.util.module_from_spec(spec);spec.loader.exec_module(g);generated=a.output/'pxa_grid.c';g.generate(source,generated);replacement=(compat/'fill_region_stack.c.inc').read_text();assert generated.read_text().replace(replacement,g.ORIGINAL_FUNCTION,1)==source.read_text()
negative=[]
for name,text,override in [('changed-source',source.read_text()+'\n',False),('duplicate-function',source.read_text()+g.ORIGINAL_FUNCTION,True)]:
 bad=a.output/(name+'.c');bad.write_text(text)
 if override:g.EXPECTED_SOURCE_SHA256=hashlib.sha256(bad.read_bytes()).hexdigest()
 output=a.output/(name+'-output.c')
 try:g.generate(bad,output)
 except SystemExit as e:assert not output.exists();negative.append({'case':name,'rejected':True,'error':str(e)})
 else:raise AssertionError('Generator accepted '+name)
original=g.ORIGINAL_FUNCTION.replace('fillContiguousRegion','oracle_fillContiguousRegion').replace('{\n','{\n    oracle_enter(x,y);\n',1).replace('    return numberOfCells;','    --oracle_depth;\n    return numberOfCells;')
adapter=replacement[replacement.index('static short fillContiguousRegion('):].replace('fillContiguousRegion','adapter_fillContiguousRegion');adapter=adapter.replace('    grid[x][y] = fillValue;','    record_visit(x,y);\n    grid[x][y] = fillValue;',1).replace('            grid[newX][newY] = fillValue;','            record_visit(newX,newY);\n            grid[newX][newY] = fillValue;',1).replace('    unsigned frame_count = 1;','    unsigned frame_count = 1;\n    observe_frames(frame_count);',1).replace('            ++frame_count;','            ++frame_count;\n            observe_frames(frame_count);',1)
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
#define CHECK(c) do {if(!(c)){fprintf(stderr,"check failed line=%d case=%u\n",__LINE__,cases);exit(2);}}while(0)
#define brogueAssert(c) CHECK(c)
#define coordinatesAreInMap(x,y) ((x)>=0 && (x)<DCOLS && (y)>=0 && (y)<DROWS)
enum directions {D0,D1,D2,D3};
static const short nbDirs[4][2]={{0,-1},{0,1},{-1,0},{1,0}};
static unsigned cases,which,counts[2],oracle_depth,oracle_peak,adapter_peak,case_oracle_peak,case_adapter_peak;
static short trace[2][CAP][2];
static uint64_t random_state=UINT64_C(0x194acb4e61497f);
static uint32_t random32(void){random_state^=random_state<<13;random_state^=random_state>>7;random_state^=random_state<<17;return(uint32_t)random_state;}
static void record_visit(short x,short y){CHECK(counts[which]<CAP);trace[which][counts[which]][0]=x;trace[which][counts[which]++][1]=y;}
static void oracle_enter(short x,short y){++oracle_depth;if(oracle_depth>oracle_peak)oracle_peak=oracle_depth;if(oracle_depth>case_oracle_peak)case_oracle_peak=oracle_depth;record_visit(x,y);}
static void observe_frames(unsigned n){CHECK(n<=CAP);if(n>adapter_peak)adapter_peak=n;if(n>case_adapter_peak)case_adapter_peak=n;}
struct grid{uint64_t pre;short cells[DCOLS][DROWS];uint64_t post;};
"""
suffix=r"""
static short run_case(short initial[DCOLS][DROWS],short sx,short sy,short fill) {
 struct grid a={.pre=0xab95,.post=0x537c},b={.pre=0xab95,.post=0x537c};short *ar[DCOLS],*br[DCOLS];
 memcpy(a.cells,initial,sizeof(a.cells));memcpy(b.cells,initial,sizeof(b.cells));for(int x=0;x<DCOLS;++x){ar[x]=a.cells[x];br[x]=b.cells[x];}
 counts[0]=counts[1]=0;oracle_depth=0;case_oracle_peak=case_adapter_peak=0;which=0;uint64_t rng=random_state;
 short expected=oracle_fillContiguousRegion(ar,sx,sy,fill);CHECK(!oracle_depth && random_state==rng);
 which=1;short actual=adapter_fillContiguousRegion(br,sx,sy,fill);CHECK(random_state==rng);
 CHECK(a.pre==0xab95 && a.post==0x537c && b.pre==0xab95 && b.post==0x537c);
 CHECK(expected==actual && expected==(short)counts[0]);CHECK(counts[0]==counts[1]);
 CHECK(!memcmp(a.cells,b.cells,sizeof(a.cells)));CHECK(!memcmp(trace[0],trace[1],counts[0]*sizeof(trace[0][0])));++cases;return actual;
}
int main(void){
 static short initial[DCOLS][DROWS];
 for(int x=0;x<DCOLS;++x)for(int y=0;y<DROWS;++y)initial[x][y]=1;
 CHECK(run_case(initial,0,0,2)==1); /* Upstream break skips even valid later neighbors. */
 for(int x=0;x<DCOLS;++x){run_case(initial,x,0,2);run_case(initial,x,DROWS-1,3);}
 for(int y=0;y<DROWS;++y){run_case(initial,0,y,4);run_case(initial,DCOLS-1,y,5);}
 run_case(initial,DCOLS/2,DROWS/2,2);
 memset(initial,0,sizeof(initial));for(int x=1;x<DCOLS-1;++x)for(int y=1;y<DROWS-1;++y)initial[x][y]=1;
 CHECK(run_case(initial,1,1,2)==(DCOLS-2)*(DROWS-2));
 CHECK(case_oracle_peak==(DCOLS-2)*(DROWS-2) && case_adapter_peak==case_oracle_peak);
 memset(initial,0,sizeof(initial));
 for(int y=1;y<DROWS-1;++y)for(int x=1;x<DCOLS-1;++x)initial[x][y]=(y%2==1 || (((y-1)/2)%2==0?x==DCOLS-2:x==1));
 run_case(initial,1,1,2);
 memset(initial,0,sizeof(initial));initial[1][1]=initial[DCOLS-2][DROWS-2]=1;run_case(initial,1,1,2);run_case(initial,DCOLS-2,DROWS-2,3);
 for(unsigned n=0;n<2048;++n){unsigned density=n%101;for(int x=0;x<DCOLS;++x)for(int y=0;y<DROWS;++y)initial[x][y]=random32()%100<density?1:(random32()%2?0:-2);short fills[]={2,3,7,0,-3};run_case(initial,random32()%DCOLS,random32()%DROWS,fills[n%5]);}
 printf("{\"cases\":%u,\"map_cells_capacity\":%u,\"oracle_max_depth\":%u,\"adapter_max_frames\":%u,\"grid_full_visit_order_and_return_count_equal\":true,\"boundary_break_preserved\":true}\n",cases,CAP,oracle_peak,adapter_peak);
 return 0;
}
"""
harness=a.output/'grid-stack-equivalence.c';harness.write_text(prefix+original+'\n'+adapter+'\n'+suffix);command=[a.cc,'-std=c11','-O2','-g','-fno-optimize-sibling-calls','-fsanitize=address,undefined','-fno-omit-frame-pointer',str(harness),'-o',str(a.output/'grid-stack-equivalence')]
with(a.output/'compile.log').open('w')as log:subprocess.run(command,stdout=log,stderr=subprocess.STDOUT,check=True)
r=subprocess.run([str(a.output/'grid-stack-equivalence')],stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=1:abort_on_error=1','UBSAN_OPTIONS':'halt_on_error=1:print_stacktrace=1'});(a.output/'run.stdout').write_text(r.stdout);(a.output/'run.stderr').write_text(r.stderr);assert r.returncode==0,(r.returncode,r.stderr[-3000:]);result={'passed':True,'native_auxiliary_test_only':True,'compiler_command':command,'upstream_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),'generated_round_trip_exact':True,'fail_closed_checks':negative,'valid_input_scope':'fillValue!=1, as sole upstream caller enforces blob labels starting2; no RNG calls in either implementation',**json.loads(r.stdout)};(a.output/'result.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))
