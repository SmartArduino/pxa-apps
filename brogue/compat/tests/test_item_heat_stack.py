#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
"""Compare the real recursive item heat DFS, including unsigned wrap/revisits."""
from pathlib import Path
import sys
sys.dont_write_bytecode = True
import argparse,hashlib,importlib.util,json,os,signal,subprocess,resource
resource.setrlimit(resource.RLIMIT_CORE,(0,0))
parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--output',type=Path,required=True);parser.add_argument('--cc',default='cc');args=parser.parse_args();args.output.mkdir(parents=True,exist_ok=False)
compat=Path(__file__).resolve().parents[1];app=compat.parent;source=app/'vendor/src/brogue/Items.c'
spec=importlib.util.spec_from_file_location('items_generator',compat/'generate_items.py');g=importlib.util.module_from_spec(spec);spec.loader.exec_module(g);generated=args.output/'pxa_items.c';g.generate(source,generated)
assert generated.read_text().replace((compat/'item_heat_stack.c.inc').read_text(),g.ORIGINAL_FUNCTION,1)==source.read_text()
negative=[]
for name,text,override in [('changed-source',source.read_text()+'\n',False),('duplicate-heat',source.read_text()+g.ORIGINAL_FUNCTION,True)]:
 bad=args.output/(name+'.c');bad.write_text(text)
 if override:g.EXPECTED_SOURCE_SHA256=hashlib.sha256(bad.read_bytes()).hexdigest()
 output=args.output/(name+'-output.c')
 try:g.generate(bad,output)
 except SystemExit as error:assert not output.exists();negative.append({'case':name,'rejected':True,'error':str(error)})
 else:raise AssertionError('Generator accepted '+name)
oracle=g.ORIGINAL_FUNCTION.replace('fillItemSpawnHeatMap','oracle_fillItemSpawnHeatMap').replace('static void oracle_fillItemSpawnHeatMap(','static void oracle_fillItemSpawnHeatMap_impl(',1).replace('heatMap[loc.x][loc.y] = heatLevel;','heatMap[loc.x][loc.y] = heatLevel;record_event(1,loc.x,loc.y,heatLevel);',1)
adapter=(compat/'item_heat_stack.c.inc').read_text();adapter=adapter[adapter.index('static unsigned short pxa_item_heat_enter('):].replace('    if (pmapAt(loc)','    record_event(0,loc.x,loc.y,heatLevel);\n    if (pmapAt(loc)',1).replace('heatMap[loc.x][loc.y] = heatLevel;','heatMap[loc.x][loc.y] = heatLevel;record_event(1,loc.x,loc.y,heatLevel);',1).replace('            --frame_count;','            record_event(2,frame->loc.x,frame->loc.y,0);\n            --frame_count;',1).replace('    while (frame_count) {','    observe_frames(frame_count);\n    while (frame_count) {',1).replace('            ++frame_count;','            ++frame_count;\n            observe_frames(frame_count);',1).replace('    free(frames);\n    brogueAssert(false);','    free(frames);\n    fprintf(stderr,"EXPECTED_ALLOCATION_FAILURE alive=%u calls=%u\\n",alloc_alive,alloc_calls);\n    brogueAssert(false);',1)
prefix=r"""
void pxa_brogue_cooperate(void) {}
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define DCOLS 79
#define DROWS 29
#define CAP (DCOLS*DROWS)
#define CHECK(c) do{if(!(c)){fprintf(stderr,"check failed line=%d case=%u\n",__LINE__,cases);exit(2);}}while(0)
#define brogueAssert(c) assert(c)
#define DUNGEON 0
#define DOOR 1
#define SECRET_DOOR 2
#define T_IS_DEEP_WATER 1
#define T_LAVA_INSTA_DEATH 2
#define T_AUTO_DESCENT 4
_Static_assert(USHRT_MAX==65535 && sizeof(unsigned short)==2,"Match reviewed Wasm unsigned-short ABI");
typedef struct {short x,y;}pos;
enum directions {UP,DOWN,LEFT,RIGHT};
static const short nbDirs[4][2]={{0,-1},{0,1},{-1,0},{1,0}};
struct pcell{short layers[1];unsigned flags;};static struct pcell(*pmap)[DROWS];
#define pmapAt(q) (&pmap[(q).x][(q).y])
static bool isPosInMap(pos q){return q.x>=0 && q.x<DCOLS && q.y>=0 && q.y<DROWS;}
static pos posNeighborInDirection(pos q,enum directions d){return(pos){q.x+nbDirs[d][0],q.y+nbDirs[d][1]};}
struct heat_grid{uint64_t pre;unsigned short cells[DCOLS][DROWS];uint64_t post;};
struct event{int kind,x,y,value;};
static FILE *trace_file;static unsigned cases,which,counts[2],oracle_depth,oracle_peak,adapter_peak,alloc_calls,alloc_alive,alloc_fail_at,allocation_peak;
static char (*passability)[DROWS];
static uint64_t random_state=UINT64_C(0x1f967e3b);
static uint32_t random32(void){random_state^=random_state<<13;random_state^=random_state>>7;random_state^=random_state<<17;return(uint32_t)random_state;}
static void record_event(int kind,short x,short y,int value){if(alloc_fail_at)return;struct event value_event={kind,x,y,value};if(which==0){CHECK(fwrite(&value_event,sizeof(value_event),1,trace_file)==1);}else{struct event expected;CHECK(fread(&expected,sizeof(expected),1,trace_file)==1);CHECK(!memcmp(&expected,&value_event,sizeof(expected)));}++counts[which];}
static void observe_frames(unsigned n){if(n>adapter_peak)adapter_peak=n;}
static bool cellHasTerrainFlag(pos q,unsigned flags){CHECK(isPosInMap(q));unsigned n=pmapAt(q)->flags&flags;record_event(3,q.x,q.y,n);return n!=0;}
static bool isPassableOrSecretDoor(pos q){CHECK(isPosInMap(q));record_event(4,q.x,q.y,passability[q.x][q.y]);return passability[q.x][q.y]!=0;}
static void*frame_malloc(size_t n){++alloc_calls;if(alloc_calls==alloc_fail_at)return NULL;void*p=malloc(n);if(p)++alloc_alive;return p;}
static void*frame_realloc(void*p,size_t n){CHECK(p && alloc_alive==1);++alloc_calls;if(alloc_calls==alloc_fail_at)return NULL;return realloc(p,n);}
static void frame_free(void*p){if(p){CHECK(alloc_alive==1);--alloc_alive;}free(p);}
static void oracle_fillItemSpawnHeatMap(unsigned short[DCOLS][DROWS],unsigned short,pos);
"""
wrapper=r"""
static void oracle_fillItemSpawnHeatMap(unsigned short heat[DCOLS][DROWS],unsigned short level,pos q){++oracle_depth;CHECK(oracle_depth<=100000);if(oracle_depth>oracle_peak)oracle_peak=oracle_depth;record_event(0,q.x,q.y,level);oracle_fillItemSpawnHeatMap_impl(heat,level,q);record_event(2,q.x,q.y,0);--oracle_depth;}
#define malloc frame_malloc
#define realloc frame_realloc
#define free frame_free
"""
suffix=r"""
#undef malloc
#undef realloc
#undef free
static void run(unsigned short initial[DCOLS][DROWS],struct pcell cells[DCOLS][DROWS],char pass[DCOLS][DROWS],pos q,unsigned short level){struct heat_grid a={.pre=0xa1b8,.post=0x79d7},b={.pre=0xa1b8,.post=0x79d7};memcpy(a.cells,initial,sizeof(a.cells));memcpy(b.cells,initial,sizeof(b.cells));struct pcell cells_before[DCOLS][DROWS];char pass_before[DCOLS][DROWS];memcpy(cells_before,cells,sizeof(cells_before));memcpy(pass_before,pass,sizeof(pass_before));counts[0]=counts[1]=oracle_depth=alloc_calls=0;CHECK(!alloc_alive);passability=pass;pmap=cells;trace_file=tmpfile();CHECK(trace_file);which=0;oracle_fillItemSpawnHeatMap(a.cells,level,q);CHECK(!oracle_depth);CHECK(!fflush(trace_file) && !fseek(trace_file,0,SEEK_SET));which=1;fillItemSpawnHeatMap(b.cells,level,q);CHECK(!alloc_alive);if(alloc_calls>allocation_peak)allocation_peak=alloc_calls;CHECK(a.pre==0xa1b8 && a.post==0x79d7 && b.pre==0xa1b8 && b.post==0x79d7);CHECK(!memcmp(a.cells,b.cells,sizeof(a.cells)));CHECK(!memcmp(cells_before,cells,sizeof(cells_before)));CHECK(!memcmp(pass_before,pass,sizeof(pass_before)));CHECK(counts[0]==counts[1]);CHECK(fgetc(trace_file)==EOF && !ferror(trace_file));CHECK(!fclose(trace_file));trace_file=NULL;++cases;}
int main(int argc,char**argv){static unsigned short initial[DCOLS][DROWS];static struct pcell cells[DCOLS][DROWS];static char pass[DCOLS][DROWS];memset(initial,255,sizeof(initial));memset(pass,1,sizeof(pass));
if(argc>1){alloc_fail_at=!strcmp(argv[1],"oom-initial")?1:2;for(short x=0;x<DCOLS;++x)for(short y=0;y<DROWS;++y)cells[x][y].layers[0]=SECRET_DOOR;pmap=cells;passability=pass;which=1;fillItemSpawnHeatMap(initial,0,(pos){0,0});CHECK(false);}
run(initial,cells,pass,(pos){0,0},0);CHECK(adapter_peak==CAP && oracle_peak==CAP);
for(short x=0;x<DCOLS;++x)for(short y=0;y<DROWS;++y)cells[x][y].layers[0]=SECRET_DOOR;
run(initial,cells,pass,(pos){0,0},0);CHECK(adapter_peak>CAP && adapter_peak==oracle_peak && allocation_peak>=2);
memset(cells,0,sizeof(cells));for(short x=0;x<DCOLS;++x)for(short y=0;y<DROWS;++y){cells[x][y].layers[0]=DOOR;pass[x][y]=x<12 && y<12;}run(initial,cells,pass,(pos){11,11},65530);
memset(cells,0,sizeof(cells));memset(pass,0,sizeof(pass));run(initial,cells,pass,(pos){0,0},0);run(initial,cells,pass,(pos){DCOLS-1,DROWS-1},65535);
for(unsigned k=0;k<256;++k){for(short x=0;x<DCOLS;++x)for(short y=0;y<DROWS;++y){cells[x][y].layers[0]=(random32()%11==0)?SECRET_DOOR:((random32()%5==0)?DOOR:0);cells[x][y].flags=(random32()%13==0)?(1u<<(random32()%3)):0;pass[x][y]=x<12 && y<12 && random32()%5!=0;initial[x][y]=(random32()%11==0)?random32()%65536:65535;}run(initial,cells,pass,(pos){random32()%12,random32()%12},k%3==0?65530:0);}
printf("{\"cases\":%u,\"map_cells\":%d,\"oracle_max_depth\":%u,\"adapter_max_frames\":%u,\"maximum_allocation_calls\":%u,\"checks\":\"full heat grid and exact call/write/predicate/return sequence; door wrap, active revisits beyond cell count, readonly inputs, canaries, allocation cleanup\"}\n",cases,CAP,oracle_peak,adapter_peak,allocation_peak);return 0;}
"""
c_file=args.output/'oracle.c';c_file.write_text(prefix+oracle+wrapper+adapter+suffix);exe=args.output/'oracle';command=[args.cc,'-std=c11','-O2','-g','-fno-optimize-sibling-calls','-fsanitize=address,undefined','-fno-omit-frame-pointer',str(c_file),'-o',str(exe)]
compiled=subprocess.run(command,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True);(args.output/'compile.log').write_text(compiled.stdout);assert compiled.returncode==0,compiled.stdout
env=dict(os.environ,ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1');result=subprocess.run([str(exe)],stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,env=env,timeout=120);(args.output/'run.log').write_text(result.stdout);assert result.returncode==0,result.stdout
failures=[]
for mode,call in [('oom-initial',1),('oom-growth',2)]:
 r=subprocess.run([str(exe),mode],stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,env=env,timeout=120);(args.output/(mode+'.log')).write_text(r.stdout);assert r.returncode==-signal.SIGABRT and ('EXPECTED_ALLOCATION_FAILURE alive=0 calls='+str(call)) in r.stdout,r.stdout;failures.append({'mode':mode,'exit_code':r.returncode,'frames_freed_before_failure':True})
receipt={'passed':True,'source_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),'generated_roundtrip_exact':True,'generator_fail_closed':negative,'allocation_failure_tests':failures,'compile_command':command,'compiler':subprocess.check_output([args.cc,'--version'],text=True).splitlines()[0],'oracle':json.loads(result.stdout)};(args.output/'result.json').write_text(json.dumps(receipt,indent=2)+'\n');print(json.dumps(receipt['oracle']))
