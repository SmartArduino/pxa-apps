#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
"""Check the actual upstream zone DFS against its bounded frame adapter."""
from pathlib import Path
import sys
sys.dont_write_bytecode = True
import argparse,hashlib,importlib.util,json,os,subprocess
parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--output',type=Path,required=True);parser.add_argument('--cc',default='cc');args=parser.parse_args();args.output.mkdir(parents=True,exist_ok=False)
compat=Path(__file__).resolve().parents[1];app=compat.parent;source=app/'vendor/src/brogue/Architect.c'
spec=importlib.util.spec_from_file_location('architect_generator',compat/'generate_architect.py');g=importlib.util.module_from_spec(spec);spec.loader.exec_module(g)
generated=args.output/'pxa_architect.c';g.generate(source,generated);restored=generated.read_text()
for original,filename,_ in g.pinned_adaptations():restored=restored.replace((compat/filename).read_text(),original,1)
assert restored==source.read_text()
negative=[]
for name,text,override in [('changed-source',source.read_text()+'\n',False),('duplicate-connect',source.read_text()+g.CONNECT_ORIGINAL_FUNCTION,True)]:
 bad=args.output/(name+'.c');bad.write_text(text)
 if override:g.EXPECTED_SOURCE_SHA256=hashlib.sha256(bad.read_bytes()).hexdigest()
 output=args.output/(name+'-output.c')
 try:g.generate(bad,output)
 except SystemExit as error:assert not output.exists();negative.append({'case':name,'rejected':True,'error':str(error)})
 else:raise AssertionError('Generator accepted '+name)
oracle=g.CONNECT_ORIGINAL_FUNCTION.replace('connectCell','oracle_connectCell').replace('static short oracle_connectCell(','static short oracle_connectCell_impl(',1).replace('zoneMap[x][y] = zoneLabel;','zoneMap[x][y] = zoneLabel;record_event(1,x,y,(char)zoneLabel);',1)
adapter=(compat/'connect_cell_stack.c.inc').read_text();adapter=adapter[adapter.index('static short connectCell('):].replace('    zoneMap[x][y] = zoneLabel;','    record_event(0,x,y,zoneLabel);\n    zoneMap[x][y] = zoneLabel;record_event(1,x,y,(char)zoneLabel);',1).replace('            zoneMap[newX][newY] = zoneLabel;','            record_event(0,newX,newY,zoneLabel);\n            zoneMap[newX][newY] = zoneLabel;record_event(1,newX,newY,(char)zoneLabel);',1).replace('            --frame_count;','            record_event(2,frame->x,frame->y,size);\n            --frame_count;',1).replace('    frames[0] =','    observe_frames(frame_count);\n    frames[0] =',1).replace('            ++frame_count;','            ++frame_count;\n            observe_frames(frame_count);',1)
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
#define EVENTS (8*CAP+8)
#define CHECK(c) do{if(!(c)){fprintf(stderr,"check failed line=%d case=%u\n",__LINE__,cases);exit(2);}}while(0)
#define brogueAssert(c) CHECK(c)
#define coordinatesAreInMap(x,y) ((x)>=0 && (x)<DCOLS && (y)>=0 && (y)<DROWS)
_Static_assert(SHRT_MAX==32767 && sizeof(short)==2,"Match reviewed Wasm short ABI");
enum directions {UP,DOWN,LEFT,RIGHT};
static const short nbDirs[4][2]={{0,-1},{0,1},{-1,0},{1,0}};
struct char_grid{uint64_t pre;char cells[DCOLS][DROWS];uint64_t post;};
struct event{int kind,x,y,value;};
static struct event trace[2][EVENTS];static unsigned cases,which,counts[2],oracle_depth,oracle_peak,adapter_peak;
static char (*passability)[DROWS];
static uint64_t random_state=UINT64_C(0x567bc93a);
static uint32_t random32(void){random_state^=random_state<<13;random_state^=random_state>>7;random_state^=random_state<<17;return(uint32_t)random_state;}
static void record_event(int kind,short x,short y,int value){CHECK(counts[which]<EVENTS);trace[which][counts[which]++]=(struct event){kind,x,y,value};}
static void observe_frames(unsigned n){CHECK(n<=CAP);if(n>adapter_peak)adapter_peak=n;}
static bool cellIsPassableOrDoor(short x,short y){CHECK(coordinatesAreInMap(x,y));record_event(3,x,y,passability[x][y]);return passability[x][y]!=0;}
static short oracle_connectCell(short,short,short,char[DCOLS][DROWS],char[DCOLS][DROWS]);
"""
wrapper=r"""
static short oracle_connectCell(short x,short y,short label,char block[DCOLS][DROWS],char zones[DCOLS][DROWS]){++oracle_depth;CHECK(oracle_depth<=CAP);if(oracle_depth>oracle_peak)oracle_peak=oracle_depth;record_event(0,x,y,label);short n=oracle_connectCell_impl(x,y,label,block,zones);record_event(2,x,y,n);--oracle_depth;return n;}
"""
suffix=r"""
static void run(char initial[DCOLS][DROWS],char block[DCOLS][DROWS],char pass[DCOLS][DROWS],short x,short y,short label){struct char_grid a={.pre=0xa1b8,.post=0x79d7},b={.pre=0xa1b8,.post=0x79d7};memcpy(a.cells,initial,sizeof(a.cells));memcpy(b.cells,initial,sizeof(b.cells));char block_before[DCOLS][DROWS],pass_before[DCOLS][DROWS];if(block)memcpy(block_before,block,sizeof(block_before));memcpy(pass_before,pass,sizeof(pass_before));counts[0]=counts[1]=oracle_depth=0;passability=pass;which=0;short expected=oracle_connectCell(x,y,label,block,a.cells);CHECK(!oracle_depth);which=1;short actual=connectCell(x,y,label,block,b.cells);CHECK(a.pre==0xa1b8 && a.post==0x79d7 && b.pre==0xa1b8 && b.post==0x79d7);CHECK(expected==actual);CHECK(!memcmp(a.cells,b.cells,sizeof(a.cells)));if(block)CHECK(!memcmp(block_before,block,sizeof(block_before)));CHECK(!memcmp(pass_before,pass,sizeof(pass_before)));CHECK(counts[0]==counts[1]);CHECK(!memcmp(trace[0],trace[1],counts[0]*sizeof(trace[0][0])));++cases;}
int main(void){static char initial[DCOLS][DROWS],block[DCOLS][DROWS],pass[DCOLS][DROWS];memset(pass,1,sizeof(pass));short labels[]={1,127,128,200,255};for(unsigned k=0;k<sizeof(labels)/sizeof(labels[0]);++k)run(initial,NULL,pass,0,0,labels[k]);CHECK(adapter_peak==CAP && oracle_peak==CAP);
for(short x=0;x<DCOLS;++x)for(short y=0;y<DROWS;++y)run(initial,block,pass,x,y,1);
memset(pass,0,sizeof(pass));run(initial,block,pass,0,0,1);run(initial,NULL,pass,DCOLS-1,DROWS-1,200);
for(short x=0;x<DCOLS;++x)for(short y=0;y<DROWS;++y)pass[x][y]=(x%2==0 || ((x%4==1 && y==DROWS-1)||(x%4==3 && y==0)));run(initial,NULL,pass,0,0,128);
memset(pass,1,sizeof(pass));for(short y=0;y<DROWS;++y)block[DCOLS/2][y]=1;run(initial,block,pass,0,0,1);run(initial,block,pass,DCOLS-1,DROWS-1,255);
for(unsigned k=0;k<512;++k){for(short x=0;x<DCOLS;++x)for(short y=0;y<DROWS;++y){pass[x][y]=(random32()%5)!=0;block[x][y]=(random32()%7)==0;initial[x][y]=(random32()%13)==0?3:0;}run(initial,k%3?block:NULL,pass,random32()%DCOLS,random32()%DROWS,labels[k%5]);}
printf("{\"cases\":%u,\"capacity\":%d,\"oracle_max_depth\":%u,\"adapter_max_frames\":%u,\"checks\":\"full zone grid, exact call/write/predicate/subtree-return order, short size, readonly inputs, canaries\"}\n",cases,CAP,oracle_peak,adapter_peak);return 0;}
"""
c_file=args.output/'oracle.c';c_file.write_text(prefix+oracle+wrapper+adapter+suffix);exe=args.output/'oracle'
command=[args.cc,'-std=c11','-O2','-g','-fno-optimize-sibling-calls','-fsanitize=address,undefined','-fno-omit-frame-pointer',str(c_file),'-o',str(exe)]
compiled=subprocess.run(command,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True);(args.output/'compile.log').write_text(compiled.stdout);assert compiled.returncode==0,compiled.stdout
result=subprocess.run([str(exe)],stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1'));(args.output/'run.log').write_text(result.stdout);assert result.returncode==0,result.stdout
receipt={'passed':True,'source_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),'generated_roundtrip_exact':True,'generator_fail_closed':negative,'compile_command':command,'compiler':subprocess.check_output([args.cc,'--version'],text=True).splitlines()[0],'oracle':json.loads(result.stdout)};(args.output/'result.json').write_text(json.dumps(receipt,indent=2)+'\n');print(json.dumps(receipt['oracle']))
