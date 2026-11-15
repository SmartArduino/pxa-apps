#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
"""Compare actual lake DFS grid writes, full visits and predicate query order."""
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
assert 'const short nbDirs[8][2] = {{0,-1}, {0,1}, {-1,0}, {1,0},' in (app/'vendor/src/brogue/GlobalsBase.c').read_text()
spec=importlib.util.spec_from_file_location('architect_generator',compat/'generate_architect.py');g=importlib.util.module_from_spec(spec);spec.loader.exec_module(g);generated=a.output/'pxa_architect.c';g.generate(source,generated);replacement=(compat/'lake_fill_stack.c.inc').read_text();room_replacement=(compat/'insert_room_stack.c.inc').read_text();restored=generated.read_text()
for original,filename,_ in g.pinned_adaptations():restored=restored.replace((compat/filename).read_text(),original,1)
assert restored==source.read_text()
negative=[]
for name,text,override in [('changed-source',source.read_text()+'\n',False),('duplicate-lake-function',source.read_text()+g.LAKE_ORIGINAL_FUNCTION,True)]:
 bad=a.output/(name+'.c');bad.write_text(text)
 if override:g.EXPECTED_SOURCE_SHA256=hashlib.sha256(bad.read_bytes()).hexdigest()
 output=a.output/(name+'-output.c')
 try:g.generate(bad,output)
 except SystemExit as e:assert not output.exists();negative.append({'case':name,'rejected':True,'error':str(e)})
 else:raise AssertionError('Generator accepted '+name)
original=g.LAKE_ORIGINAL_FUNCTION.replace('lakeFloodFill','oracle_lakeFloodFill').replace('{\n','{\n    oracle_enter(x,y);\n',1);last=original.rfind('}');original=original[:last]+'    --oracle_depth;\n'+original[last:]
adapter=replacement[replacement.index('static void lakeFloodFill('):].replace('lakeFloodFill','adapter_lakeFloodFill').replace('    floodMap[x][y] = true;','    record_visit(x,y);\n    floodMap[x][y] = true;',1).replace('            floodMap[newX][newY] = true;','            record_visit(newX,newY);\n            floodMap[newX][newY] = true;',1).replace('    unsigned frame_count = 1;','    unsigned frame_count = 1;\n    observe_frames(frame_count);',1).replace('            ++frame_count;','            ++frame_count;\n            observe_frames(frame_count);',1)
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
#define T_PATHING_BLOCKER 1ul
#define TM_CONNECTS_LEVEL 2ul
struct pos{short x,y;};typedef struct pos pos;
enum directions{D0,D1,D2,D3};
static const short nbDirs[4][2]={{0,-1},{0,1},{-1,0},{1,0}};
static unsigned cases,which,counts[2],queries[2],oracle_depth,oracle_peak,adapter_peak;
static short trace[2][CAP][2],query_trace[2][CAP*8][3];
static unsigned long terrain[DCOLS][DROWS],mechanics[DCOLS][DROWS];
static uint64_t random_state=UINT64_C(0x8217d94a37cbf);
static uint32_t random32(void){random_state^=random_state<<13;random_state^=random_state>>7;random_state^=random_state<<17;return(uint32_t)random_state;}
static void record_visit(short x,short y){CHECK(counts[which]<CAP);trace[which][counts[which]][0]=x;trace[which][counts[which]++][1]=y;}
static void oracle_enter(short x,short y){++oracle_depth;if(oracle_depth>oracle_peak)oracle_peak=oracle_depth;record_visit(x,y);}
static void observe_frames(unsigned n){CHECK(n<=CAP);if(n>adapter_peak)adapter_peak=n;}
static void query(short kind,pos p){CHECK(queries[which]<CAP*8);query_trace[which][queries[which]][0]=kind;query_trace[which][queries[which]][1]=p.x;query_trace[which][queries[which]++][2]=p.y;}
static bool cellHasTerrainFlag(pos p,unsigned long mask){query(1,p);return(terrain[p.x][p.y]&mask)!=0;}
static bool cellHasTMFlag(pos p,unsigned long mask){query(2,p);return(mechanics[p.x][p.y]&mask)!=0;}
struct grid{uint64_t pre;short cells[DCOLS][DROWS];uint64_t post;};
"""
suffix=r"""
static void run_case(short flood[DCOLS][DROWS],short map[DCOLS][DROWS],short lake[DCOLS][DROWS],short dx,short dy,short sx,short sy,int alias){
 struct grid a[3],b[3];short *ar[3][DCOLS],*br[3][DCOLS];
 for(int n=0;n<3;++n){a[n].pre=b[n].pre=0x174ab8;a[n].post=b[n].post=0x91af3c;short(*initial)[DROWS]=n==0?flood:n==1?map:lake;memcpy(a[n].cells,initial,sizeof(a[n].cells));memcpy(b[n].cells,initial,sizeof(b[n].cells));for(int x=0;x<DCOLS;++x){ar[n][x]=a[n].cells[x];br[n][x]=b[n].cells[x];}}
 counts[0]=counts[1]=queries[0]=queries[1]=0;oracle_depth=0;which=0;
 oracle_lakeFloodFill(sx,sy,ar[0],ar[alias==1?0:1],ar[alias==2?0:2],dx,dy);CHECK(!oracle_depth);
 which=1;adapter_lakeFloodFill(sx,sy,br[0],br[alias==1?0:1],br[alias==2?0:2],dx,dy);
 for(int n=0;n<3;++n){CHECK(a[n].pre==0x174ab8 && b[n].pre==0x174ab8 && a[n].post==0x91af3c && b[n].post==0x91af3c);CHECK(!memcmp(a[n].cells,b[n].cells,sizeof(a[n].cells)));}
 CHECK(counts[0]==counts[1]);CHECK(!memcmp(trace[0],trace[1],counts[0]*sizeof(trace[0][0])));CHECK(queries[0]==queries[1]);CHECK(!memcmp(query_trace[0],query_trace[1],queries[0]*sizeof(query_trace[0][0])));++cases;
}
int main(void){
 static short flood[DCOLS][DROWS],map[DCOLS][DROWS],lake[DCOLS][DROWS];
 run_case(flood,map,lake,0,0,0,0,0);CHECK(counts[0]==CAP && oracle_peak==CAP && adapter_peak==CAP);
 run_case(flood,map,lake,0,0,DCOLS-1,DROWS-1,0);
 for(int x=0;x<DCOLS;++x)for(int y=0;y<DROWS;++y){terrain[x][y]=T_PATHING_BLOCKER;mechanics[x][y]=TM_CONNECTS_LEVEL;}
 run_case(flood,map,lake,0,0,0,0,0);CHECK(counts[0]==CAP);
 memset(mechanics,0,sizeof(mechanics));run_case(flood,map,lake,0,0,0,0,0);CHECK(counts[0]==1);
 memset(terrain,0,sizeof(terrain));
 for(int x=0;x<DCOLS;++x)for(int y=0;y<DROWS;++y)lake[x][y]=!(y%2==0 || ((y/2)%2==0?x==DCOLS-1:x==0));
 run_case(flood,map,lake,0,0,0,0,0);memset(lake,0,sizeof(lake));
 for(int x=0;x<DCOLS;++x)for(int y=0;y<DROWS;++y)map[x][y]=1;
 run_case(flood,map,lake,DCOLS,0,0,0,0);CHECK(counts[0]==CAP);
 run_case(flood,map,lake,-DCOLS,0,0,0,0);CHECK(counts[0]==CAP);
 memset(map,0,sizeof(map));
 for(int dx=-2;dx<=2;++dx)for(int dy=-2;dy<=2;++dy){run_case(flood,map,lake,dx,dy,0,0,0);run_case(flood,map,lake,dx,dy,DCOLS-1,DROWS-1,0);}
 for(unsigned n=0;n<2048;++n){unsigned density=n%101;
  for(int x=0;x<DCOLS;++x)for(int y=0;y<DROWS;++y){flood[x][y]=random32()%11==0?1:0;map[x][y]=random32()%100<density?1:0;lake[x][y]=random32()%100<density?1:0;terrain[x][y]=random32()%2?T_PATHING_BLOCKER:0;mechanics[x][y]=random32()%4==0?TM_CONNECTS_LEVEL:0;}
  short dx=(short)(random32()%11)-5,dy=(short)(random32()%11)-5,sx=random32()%DCOLS,sy=random32()%DROWS;run_case(flood,map,lake,dx,dy,sx,sy,0);if(n%32==0){run_case(flood,map,lake,dx,dy,sx,sy,1);run_case(flood,map,lake,dx,dy,sx,sy,2);}
 }
 printf("{\"cases\":%u,\"map_cells_capacity\":%u,\"oracle_max_depth\":%u,\"adapter_max_frames\":%u,\"all_grid_writes_full_visit_and_predicate_order_equal\":true,\"terrain_TM_short_circuit_and_outside_grid_semantics_preserved\":true}\n",cases,CAP,oracle_peak,adapter_peak);return 0;
}
"""
harness=a.output/'lake-stack-equivalence.c';harness.write_text(prefix+original+'\n'+adapter+'\n'+suffix);command=[a.cc,'-std=c11','-O2','-g','-fno-optimize-sibling-calls','-fsanitize=address,undefined','-fno-omit-frame-pointer',str(harness),'-o',str(a.output/'lake-stack-equivalence')]
with(a.output/'compile.log').open('w')as log:subprocess.run(command,stdout=log,stderr=subprocess.STDOUT,check=True)
r=subprocess.run([str(a.output/'lake-stack-equivalence')],stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=1:abort_on_error=1','UBSAN_OPTIONS':'halt_on_error=1:print_stacktrace=1'});(a.output/'run.stdout').write_text(r.stdout);(a.output/'run.stderr').write_text(r.stderr);assert r.returncode==0,(r.returncode,r.stderr[-3000:]);result={'passed':True,'native_auxiliary_test_only':True,'compiler_command':command,'upstream_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),'generated_round_trip_exact':True,'fail_closed_checks':negative,'predicate_scope':'Actual original and adapter bodies call the same instrumented pure flag-reader fixtures. Tests compare full predicate call sequence and all Boolean branches; real game APIs are covered by original SDK acceptance.',**json.loads(r.stdout)};(a.output/'result.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))
