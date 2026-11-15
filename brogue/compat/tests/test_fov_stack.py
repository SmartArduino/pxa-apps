#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
"""Compare actual shadowcasting grids and all recursive/predicate/write events."""
from pathlib import Path
import sys
sys.dont_write_bytecode = True
import argparse,hashlib,importlib.util,json,os,re,subprocess
parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--output',type=Path,required=True);parser.add_argument('--cc',default='cc');args=parser.parse_args();args.output.mkdir(parents=True,exist_ok=False)
compat=Path(__file__).resolve().parents[1];app=compat.parent;source=app/'vendor/src/brogue/Movement.c'
spec=importlib.util.spec_from_file_location('movement_generator',compat/'generate_movement.py');g=importlib.util.module_from_spec(spec);spec.loader.exec_module(g);generated=args.output/'pxa_movement.c';g.generate(source,generated);assert generated.read_text().replace((compat/'fov_stack.c.inc').read_text(),g.ORIGINAL_FUNCTION,1)==source.read_text()
negative=[]
for name,text,override in [('changed-source',source.read_text()+'\n',False),('duplicate-fov',source.read_text()+g.ORIGINAL_FUNCTION,True)]:
 bad=args.output/(name+'.c');bad.write_text(text)
 if override:g.EXPECTED_SOURCE_SHA256=hashlib.sha256(bad.read_bytes()).hexdigest()
 output=args.output/(name+'-output.c')
 try:g.generate(bad,output)
 except SystemExit as error:assert not output.exists();negative.append({'case':name,'rejected':True,'error':str(error)})
 else:raise AssertionError('Generator accepted '+name)
def wasm_long_types(text):
 text=text.replace('unsigned long','uint32_t').replace('long int','int32_t');return re.sub(r'\blong\b','int32_t',text)
oracle=g.ORIGINAL_FUNCTION.replace('scanOctantFOV','oracle_scanOctantFOV').replace('void oracle_scanOctantFOV(','static void oracle_scanOctantFOV_impl(',1).replace('grid[x][y] = 1;','grid[x][y] = 1;record_event(1,x,y,1);')
adapter=(compat/'fov_stack.c.inc').read_text();adapter=adapter[adapter.index('void scanOctantFOV('):].replace('            short a, b, iStart;','            short a, b, iStart;\n            record_event(0,frame->column,frame->start_slope,frame->end_slope);',1).replace('--frame_count;','record_event(2,frame->column,frame->start_slope,frame->end_slope);--frame_count;').replace('grid[x][y] = 1;','grid[x][y] = 1;record_event(1,x,y,1);').replace('    while (frame_count) {','    observe_frames(frame_count);\n    while (frame_count) {',1).replace('                    ++frame_count;','                    ++frame_count;observe_frames(frame_count);').replace('                ++frame_count;','                ++frame_count;observe_frames(frame_count);')
movement=source.read_text();i=movement.index('void betweenOctant1andN(');transform=movement[i:movement.index('\n}\n',i)+3].replace('void betweenOctant1andN(','static void original_transform(',1)
math=(app/'vendor/src/brogue/Math.c').read_text();math_helpers=''
for signature in ['static int msbpos(','static fixpt fp_exp2(','fixpt fp_sqrt(']:
 i=math.index(signature);math_helpers+=math[i:math.index('\n}\n',i)+3]+'\n'
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
#define CAP (DCOLS+DROWS+1)
#define EVENTS 100000
#define CHECK(c) do{if(!(c)){fprintf(stderr,"check failed line=%d case=%u\n",__LINE__,cases);exit(2);}}while(0)
#define brogueAssert(c) CHECK(c)
#define coordinatesAreInMap(x,y) ((x)>=0 && (x)<DCOLS && (y)>=0 && (y)<DROWS)
#define min(a,b) ((a)<(b)?(a):(b))
#define max(a,b) ((a)>(b)?(a):(b))
#define IN_FIELD_OF_VIEW UINT32_C(0x10)
#define FP_BASE 16
#define FP_FACTOR (1LL<<FP_BASE)
#define FP_MUL(x,y) ((x)*(y)/FP_FACTOR)
#define LOS_SLOPE_GRANULARITY 32768
_Static_assert(SHRT_MAX==32767 && sizeof(short)==2 && INT32_MAX==2147483647,"Match reviewed Wasm integer ABI");
typedef int64_t fixpt;typedef bool boolean;typedef struct{short x,y;}pos;
struct pcell{uint32_t flags,terrain;};static struct pcell(*pmap)[DROWS];
struct char_grid{uint64_t pre;char cells[DCOLS][DROWS];uint64_t post;};
struct event{int64_t kind,x,y,value;};
static struct event trace[2][EVENTS];static unsigned cases,which,counts[2],oracle_depth,oracle_peak,adapter_peak;
static uint64_t random_state=UINT64_C(0x1be693fa);
static uint32_t random32(void){random_state^=random_state<<13;random_state^=random_state>>7;random_state^=random_state<<17;return(uint32_t)random_state;}
static void record_event(int kind,int x,int y,int64_t value){CHECK(counts[which]<EVENTS);trace[which][counts[which]++]=(struct event){kind,x,y,value};}
static void observe_frames(unsigned n){CHECK(n<=CAP);if(n>adapter_peak)adapter_peak=n;}
static bool cellHasTerrainFlag(pos q,uint32_t flags){CHECK(coordinatesAreInMap(q.x,q.y));uint32_t n=pmap[q.x][q.y].terrain&flags;record_event(3,q.x,q.y,n);return n!=0;}
static void oracle_scanOctantFOV(char[DCOLS][DROWS],short,short,short,fixpt,short,int32_t,int32_t,uint32_t,uint32_t,boolean);
"""
transform_wrapper=r"""
static void betweenOctant1andN(short*x,short*y,short x0,short y0,short n){record_event(4,*x,*y,n);original_transform(x,y,x0,y0,n);record_event(5,*x,*y,n);}
"""
wrapper=r"""
static void oracle_scanOctantFOV(char grid[DCOLS][DROWS],short x,short y,short octant,fixpt radius,short column,int32_t start,int32_t end,uint32_t terrain,uint32_t flags,boolean cautious){++oracle_depth;CHECK(oracle_depth<=CAP);if(oracle_depth>oracle_peak)oracle_peak=oracle_depth;record_event(0,column,start,end);oracle_scanOctantFOV_impl(grid,x,y,octant,radius,column,start,end,terrain,flags,cautious);record_event(2,column,start,end);--oracle_depth;}
"""
suffix=r"""
static void run(char initial[DCOLS][DROWS],struct pcell cells[DCOLS][DROWS],short x,short y,short octant,fixpt radius,uint32_t terrain,uint32_t flags,boolean cautious){struct char_grid a={.pre=0xabc8,.post=0x79e7},b={.pre=0xabc8,.post=0x79e7};memcpy(a.cells,initial,sizeof(a.cells));memcpy(b.cells,initial,sizeof(b.cells));struct pcell before[DCOLS][DROWS];memcpy(before,cells,sizeof(before));counts[0]=counts[1]=oracle_depth=0;pmap=cells;which=0;oracle_scanOctantFOV(a.cells,x,y,octant,radius,1,-LOS_SLOPE_GRANULARITY,0,terrain,flags,cautious);CHECK(!oracle_depth);which=1;scanOctantFOV(b.cells,x,y,octant,radius,1,-LOS_SLOPE_GRANULARITY,0,terrain,flags,cautious);CHECK(a.pre==0xabc8 && a.post==0x79e7 && b.pre==0xabc8 && b.post==0x79e7);CHECK(!memcmp(a.cells,b.cells,sizeof(a.cells)));CHECK(!memcmp(before,cells,sizeof(before)));CHECK(counts[0]==counts[1]);CHECK(!memcmp(trace[0],trace[1],counts[0]*sizeof(trace[0][0])));++cases;}
int main(void){static char initial[DCOLS][DROWS];static struct pcell cells[DCOLS][DROWS];pos origins[]={{0,0},{0,DROWS-1},{DCOLS-1,0},{DCOLS-1,DROWS-1},{DCOLS/2,DROWS/2}};fixpt radii[]={0,FP_FACTOR/2,FP_FACTOR,3*FP_FACTOR/2,2*FP_FACTOR,29*FP_FACTOR/4,29*FP_FACTOR/2,31*FP_FACTOR,80*FP_FACTOR,109*FP_FACTOR,130*FP_FACTOR};
for(unsigned pattern=0;pattern<3;++pattern){for(short x=0;x<DCOLS;++x)for(short y=0;y<DROWS;++y){cells[x][y].terrain=pattern==0?0:pattern==1?((x+y)%3==0):((x%7==0) || (y%5==0));cells[x][y].flags=((x+y)%2==0?IN_FIELD_OF_VIEW:0)|((x*y)%11==0?2:0);}for(unsigned q=0;q<5;++q)for(unsigned r=0;r<sizeof(radii)/sizeof(radii[0]);++r)for(short octant=1;octant<=8;++octant)for(int cautious=0;cautious<2;++cautious)run(initial,cells,origins[q].x,origins[q].y,octant,radii[r],1,pattern==2?2:0,cautious);}
for(unsigned k=0;k<512;++k){for(short x=0;x<DCOLS;++x)for(short y=0;y<DROWS;++y){initial[x][y]=random32()%3;cells[x][y].terrain=random32()%16;cells[x][y].flags=(random32()%2?IN_FIELD_OF_VIEW:0)|(random32()%9==0?2:0);}run(initial,cells,random32()%DCOLS,random32()%DROWS,1+random32()%8,(random32()%(130*65536)),random32()%16,k%2?2:0,k%3==0);}
CHECK(oracle_peak==adapter_peak && oracle_peak<DCOLS+DROWS+1 && oracle_peak>=DCOLS-1);printf("{\"cases\":%u,\"capacity\":%d,\"oracle_max_depth\":%u,\"adapter_max_frames\":%u,\"checks\":\"full visibility grid and call/return/transform/predicate/write order; all8octants, corners, fractional radii, cautious walls, random masks; exact upstream integer sqrt; Wasm32 long type model\"}\n",cases,CAP,oracle_peak,adapter_peak);return 0;}
"""
c_file=args.output/'oracle.c';c_file.write_text(prefix+math_helpers+transform+transform_wrapper+wasm_long_types(oracle)+wrapper+wasm_long_types(adapter)+suffix);exe=args.output/'oracle';command=[args.cc,'-std=c11','-O2','-g','-fno-optimize-sibling-calls','-fsanitize=address,undefined','-fno-omit-frame-pointer',str(c_file),'-o',str(exe)]
compiled=subprocess.run(command,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True);(args.output/'compile.log').write_text(compiled.stdout);assert compiled.returncode==0,compiled.stdout
result=subprocess.run([str(exe)],stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=120,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1'));(args.output/'run.log').write_text(result.stdout);assert result.returncode==0,result.stdout
receipt={'passed':True,'source_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),'math_source_sha256':hashlib.sha256((app/'vendor/src/brogue/Math.c').read_bytes()).hexdigest(),'wasm32_long_types_modeled_explicitly':True,'generated_roundtrip_exact':True,'generator_fail_closed':negative,'compile_command':command,'compiler':subprocess.check_output([args.cc,'--version'],text=True).splitlines()[0],'oracle':json.loads(result.stdout)};(args.output/'result.json').write_text(json.dumps(receipt,indent=2)+'\n');print(json.dumps(receipt['oracle']))
