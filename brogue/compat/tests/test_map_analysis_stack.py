#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
"""Compare actual audit/count DFS calls, writes, per-call returns and grids."""
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
assert 'const short nbDirs[8][2] = {{0,-1}, {0,1}, {-1,0}, {1,0}, {-1,-1}, {-1,1}, {1,-1}, {1,1}};' in (app/'vendor/src/brogue/GlobalsBase.c').read_text()
spec=importlib.util.spec_from_file_location('architect_generator',compat/'generate_architect.py');g=importlib.util.module_from_spec(spec);spec.loader.exec_module(g);generated=a.output/'pxa_architect.c';g.generate(source,generated);restored=generated.read_text()
for original,filename,_ in g.pinned_adaptations():restored=restored.replace((compat/filename).read_text(),original,1)
assert restored==source.read_text()
negative=[]
for name,text,override in [('changed-source',source.read_text()+'\n',False),('duplicate-audit',source.read_text()+g.AUDIT_ORIGINAL_FUNCTION,True),('duplicate-count',source.read_text()+g.FLOOD_COUNT_ORIGINAL_FUNCTION,True)]:
 bad=a.output/(name+'.c');bad.write_text(text)
 if override:g.EXPECTED_SOURCE_SHA256=hashlib.sha256(bad.read_bytes()).hexdigest()
 output=a.output/(name+'-output.c')
 try:g.generate(bad,output)
 except SystemExit as e:assert not output.exists();negative.append({'case':name,'rejected':True,'error':str(e)})
 else:raise AssertionError('Generator accepted '+name)
oracle_audit=g.AUDIT_ORIGINAL_FUNCTION.replace('auditLoop','oracle_auditLoop').replace('static void oracle_auditLoop(','static void oracle_auditLoop_impl(',1).replace('grid[x][y] = true;','grid[x][y] = true;record_event(1,x,y,1);',1)
audit=(compat/'audit_loop_stack.c.inc').read_text();audit=audit[audit.index('static boolean pxa_mark_audit_cell('):].replace('{\n','{\n    record_event(0,x,y,0);\n',1).replace('grid[x][y] = true;','grid[x][y] = true;record_event(1,x,y,1);',1).replace('    frames[0] =','    observe_frames(frame_count);\n    frames[0] =',1).replace('            ++frame_count;','            ++frame_count;\n            observe_frames(frame_count);',1)
oracle_count=g.FLOOD_COUNT_ORIGINAL_FUNCTION.replace('floodFillCount','oracle_floodFillCount').replace('static short oracle_floodFillCount(','static short oracle_floodFillCount_impl(',1).replace('results[startX][startY] = true;','results[startX][startY] = true;record_event(1,startX,startY,1);',1)
count=(compat/'flood_count_stack.c.inc').read_text();count=count[count.index('static short pxa_flood_count_initial('):].replace('{\n','{\n    record_event(0,x,y,0);\n',1).replace('results[startX][startY] = true;','results[startX][startY] = true;record_event(1,startX,startY,1);',1).replace('results[newX][newY] = true;','results[newX][newY] = true;record_event(1,newX,newY,1);',1).replace('            --frame_count;','            record_event(2,frame->x,frame->y,count);\n            --frame_count;',1).replace('    frames[0] =','    observe_frames(frame_count);\n    frames[0] =',1).replace('            ++frame_count;','            ++frame_count;\n            observe_frames(frame_count);',1)
prefix=r"""
void pxa_brogue_cooperate(void) {}
#include <stdbool.h>
#include <stdint.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define DCOLS 79
#define DROWS 29
#define CAP (DCOLS*DROWS)
#define EVENTS (9*CAP+1)
#define DIRECTION_COUNT 8
#define IN_LOOP UINT32_C(0x10)
#define IS_IN_AREA_MACHINE UINT32_C(0x20)
#define min(a,b) ((a)<(b)?(a):(b))
#define CHECK(c) do{if(!(c)){fprintf(stderr,"check failed line=%d case=%u\n",__LINE__,cases);exit(2);}}while(0)
#define brogueAssert(c) CHECK(c)
#define coordinatesAreInMap(x,y) ((x)>=0 && (x)<DCOLS && (y)>=0 && (y)<DROWS)
_Static_assert(SHRT_MAX==32767 && sizeof(short)==2,"Match reviewed Wasm short ABI");
static const short nbDirs[8][2]={{0,-1},{0,1},{-1,0},{1,0},{-1,-1},{-1,1},{1,-1},{1,1}};
typedef bool boolean;
struct pcell{uint32_t flags;};static struct pcell(*pmap)[DROWS];
struct char_grid{uint64_t pre;char cells[DCOLS][DROWS];uint64_t post;};
struct event{int kind,x,y,value;};
static struct event trace[2][EVENTS];static unsigned cases,which,counts[2],oracle_depth,oracle_peak,adapter_peak;
static uint64_t random_state=UINT64_C(0x9a3f21bc4);
static uint32_t random32(void){random_state^=random_state<<13;random_state^=random_state>>7;random_state^=random_state<<17;return(uint32_t)random_state;}
static void record_event(int kind,short x,short y,int value){CHECK(counts[which]<EVENTS);trace[which][counts[which]++]=(struct event){kind,x,y,value};}
static void observe_frames(unsigned n){CHECK(n<=CAP);if(n>adapter_peak)adapter_peak=n;}
static void oracle_auditLoop(short,short,char[DCOLS][DROWS]);
static short oracle_floodFillCount(char[DCOLS][DROWS],char[DCOLS][DROWS],short,short);
"""
wrappers=r"""
static void oracle_auditLoop(short x,short y,char grid[DCOLS][DROWS]){++oracle_depth;CHECK(oracle_depth<=CAP+1);if(oracle_depth>oracle_peak)oracle_peak=oracle_depth;record_event(0,x,y,0);oracle_auditLoop_impl(x,y,grid);--oracle_depth;}
static short oracle_floodFillCount(char r[DCOLS][DROWS],char p[DCOLS][DROWS],short x,short y){++oracle_depth;CHECK(oracle_depth<=CAP);if(oracle_depth>oracle_peak)oracle_peak=oracle_depth;record_event(0,x,y,0);short n=oracle_floodFillCount_impl(r,p,x,y);record_event(2,x,y,n);--oracle_depth;return n;}
"""
suffix=r"""
static void run_audit(char initial[DCOLS][DROWS],struct pcell flags[DCOLS][DROWS],short x,short y){struct char_grid a={.pre=0x1ab8,.post=0x89d7},b={.pre=0x1ab8,.post=0x89d7};memcpy(a.cells,initial,sizeof(a.cells));memcpy(b.cells,initial,sizeof(b.cells));struct pcell flags_before[DCOLS][DROWS];memcpy(flags_before,flags,sizeof(flags_before));counts[0]=counts[1]=oracle_depth=0;pmap=flags;which=0;oracle_auditLoop(x,y,a.cells);CHECK(!oracle_depth);which=1;auditLoop(x,y,b.cells);CHECK(a.pre==0x1ab8 && a.post==0x89d7 && b.pre==0x1ab8 && b.post==0x89d7);CHECK(!memcmp(a.cells,b.cells,sizeof(a.cells)));CHECK(!memcmp(flags_before,flags,sizeof(flags_before)));CHECK(counts[0]==counts[1]);CHECK(!memcmp(trace[0],trace[1],counts[0]*sizeof(trace[0][0])));++cases;}
static short run_count(char initial[DCOLS][DROWS],char pass[DCOLS][DROWS],struct pcell flags[DCOLS][DROWS],short x,short y){struct char_grid a={.pre=0x1ab8,.post=0x89d7},b={.pre=0x1ab8,.post=0x89d7};memcpy(a.cells,initial,sizeof(a.cells));memcpy(b.cells,initial,sizeof(b.cells));struct pcell flags_before[DCOLS][DROWS];memcpy(flags_before,flags,sizeof(flags_before));counts[0]=counts[1]=oracle_depth=0;pmap=flags;which=0;short expected=oracle_floodFillCount(a.cells,pass,x,y);CHECK(!oracle_depth);which=1;short actual=floodFillCount(b.cells,pass,x,y);CHECK(a.pre==0x1ab8 && a.post==0x89d7 && b.pre==0x1ab8 && b.post==0x89d7);CHECK(expected==actual);CHECK(!memcmp(a.cells,b.cells,sizeof(a.cells)));CHECK(!memcmp(flags_before,flags,sizeof(flags_before)));CHECK(counts[0]==counts[1]);CHECK(!memcmp(trace[0],trace[1],counts[0]*sizeof(trace[0][0])));++cases;return actual;}
int main(void){static char initial[DCOLS][DROWS],pass[DCOLS][DROWS];static struct pcell flags[DCOLS][DROWS];
 run_audit(initial,flags,0,0);CHECK(adapter_peak==CAP && oracle_peak==CAP+1);run_audit(initial,flags,-1,0);run_audit(initial,flags,DCOLS,0);run_audit(initial,flags,0,-1);run_audit(initial,flags,0,DROWS);
 for(int x=0;x<DCOLS;++x)for(int y=0;y<DROWS;++y)initial[x][y]=!(y%2==0 || ((y/2)%2==0?x==DCOLS-1:x==0));run_audit(initial,flags,0,0);memset(initial,0,sizeof(initial));
 for(unsigned mask=0;mask<512;++mask){memset(flags,0,sizeof(flags));for(unsigned i=0;i<9;++i)flags[DCOLS/2+(int)(i%3)-1][DROWS/2+(int)(i/3)-1].flags=mask&(1u<<i)?IN_LOOP:0;run_audit(initial,flags,DCOLS/2,DROWS/2);}
 for(unsigned n=0;n<512;++n){for(int x=0;x<DCOLS;++x)for(int y=0;y<DROWS;++y){initial[x][y]=random32()%7==0?1:0;flags[x][y].flags=random32();}run_audit(initial,flags,random32()%DCOLS,random32()%DROWS);}
 unsigned audit_cases=cases,audit_oracle_peak=oracle_peak,audit_adapter_peak=adapter_peak;oracle_peak=adapter_peak=0;memset(initial,0,sizeof(initial));memset(flags,0,sizeof(flags));memset(pass,1,sizeof(pass));CHECK(run_count(initial,pass,flags,0,0)==CAP);CHECK(oracle_peak==CAP && adapter_peak==CAP);
 memset(pass,0,sizeof(pass));short cx=DCOLS/2,cy=DROWS/2;pass[cx][cy]=1;for(int d=0;d<4;++d)for(int n=1;n<=2;++n)pass[cx+nbDirs[d][0]*n][cy+nbDirs[d][1]*n]=2;CHECK((short)40001==-25535);CHECK(run_count(initial,pass,flags,cx,cy)==(short)40001);
 for(int x=0;x<DCOLS;++x)for(int y=0;y<DROWS;++y){pass[x][y]=1;flags[x][y].flags=IS_IN_AREA_MACHINE;}run_count(initial,pass,flags,0,0);
 for(unsigned n=0;n<512;++n){for(int x=0;x<DCOLS;++x)for(int y=0;y<DROWS;++y){initial[x][y]=random32()%9==0?1:0;pass[x][y]=random32()%4;flags[x][y].flags=random32()%11==0?IS_IN_AREA_MACHINE:0;}short x=random32()%DCOLS,y=random32()%DROWS;initial[x][y]=0;if(!pass[x][y])pass[x][y]=1;run_count(initial,pass,flags,x,y);}
 printf("{\"cases\":%u,\"audit_cases\":%u,\"count_cases\":%u,\"capacity\":%u,\"audit_oracle_max_depth\":%u,\"audit_max_frames\":%u,\"count_oracle_max_depth\":%u,\"count_max_frames\":%u,\"full_grids_call_write_and_subtree_return_order_equal\":true,\"original_short_wrap_and_min10000_order_preserved\":true}\n",cases,audit_cases,cases-audit_cases,CAP,audit_oracle_peak,audit_adapter_peak,oracle_peak,adapter_peak);return 0;
}
"""
harness=a.output/'map-analysis-stack-equivalence.c';harness.write_text(prefix+oracle_audit+'\n'+oracle_count+'\n'+wrappers+'\n'+audit+'\n'+count+'\n'+suffix);command=[a.cc,'-std=c11','-O2','-g','-fno-optimize-sibling-calls','-fsanitize=address,undefined','-fno-omit-frame-pointer',str(harness),'-o',str(a.output/'map-analysis-stack-equivalence')]
with(a.output/'compile.log').open('w')as log:subprocess.run(command,stdout=log,stderr=subprocess.STDOUT,check=True)
r=subprocess.run([str(a.output/'map-analysis-stack-equivalence')],stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=1:abort_on_error=1','UBSAN_OPTIONS':'halt_on_error=1:print_stacktrace=1'});(a.output/'run.stdout').write_text(r.stdout);(a.output/'run.stderr').write_text(r.stderr);assert r.returncode==0,(r.returncode,r.stderr[-3000:]);result={'passed':True,'native_auxiliary_test_only':True,'compiler_command':command,'upstream_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),'generated_round_trip_exact':True,'fail_closed_checks':negative,'scope':'Actual upstream bodies and adapters; selected flag fixtures plus full state/call/subtree-return traces. Machine-count short truncation matches reviewed Wasm ABI; no arithmetic normalization.',**json.loads(r.stdout)};(a.output/'result.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))
