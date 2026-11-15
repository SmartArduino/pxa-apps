#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
"""Compare the actual upstream and generated DFS grid writes and visit order."""
from pathlib import Path
import sys
sys.dont_write_bytecode = True
import argparse, hashlib, importlib.util, json, os, subprocess, re

P = argparse.ArgumentParser(description=__doc__)
P.add_argument('--output', type=Path, required=True)
P.add_argument('--cc', default='cc')
a = P.parse_args()
a.output.mkdir(parents=True, exist_ok=False)
compat = Path(__file__).resolve().parents[1]
app = compat.parent
source = app / 'vendor/src/brogue/Architect.c'

rogue = (app/'vendor/src/brogue/Rogue.h').read_text()
for name, value in [('COLS','100'),('MESSAGE_LINES','3'),('STAT_BAR_WIDTH','20')]:
    assert re.search(r'^#define\s+'+name+r'\s+'+value+r'\s*(?://[^\n]*)?$',rogue,re.M), 'Map constants changed'
assert '#define ROWS                    (31 + MESSAGE_LINES)' in rogue
assert '#define DCOLS                   (COLS - STAT_BAR_WIDTH - 1)' in rogue
assert '#define DROWS                   (ROWS - MESSAGE_LINES - 2)' in rogue
globals_base = (app/'vendor/src/brogue/GlobalsBase.c').read_text()
assert 'const short nbDirs[8][2] = {{0,-1}, {0,1}, {-1,0}, {1,0},' in globals_base

spec = importlib.util.spec_from_file_location('architect_generator', compat/'generate_architect.py')
g = importlib.util.module_from_spec(spec)
spec.loader.exec_module(g)
generated = a.output/'pxa_architect.c'
g.generate(source, generated)
replacement = (compat/'insert_room_stack.c.inc').read_text()
restored=generated.read_text()
for original,filename,_ in g.pinned_adaptations():restored=restored.replace((compat/filename).read_text(),original,1)
assert restored==source.read_text()
negative = []
for name, text, override_hash in [('changed-source',source.read_text()+'\n',False),('duplicate-function',source.read_text()+g.ORIGINAL_FUNCTION,True)]:
    bad = a.output/(name+'.c'); bad.write_text(text)
    if override_hash:
        g.EXPECTED_SOURCE_SHA256 = hashlib.sha256(bad.read_bytes()).hexdigest()
    output = a.output/(name+'-output.c')
    try:
        g.generate(bad,output)
    except SystemExit as e:
        assert not output.exists()
        negative.append({'case':name,'rejected':True,'error':str(e)})
    else:
        raise AssertionError('Generator accepted '+name)
original = g.ORIGINAL_FUNCTION.replace('insertRoomAt','oracle_insertRoomAt')
original = original.replace('{\n','{\n    oracle_enter(xRoom,yRoom);\n',1)
last = original.rfind('}')
original = original[:last]+'    --oracle_depth;\n'+original[last:]
adapter = replacement[replacement.index('static void insertRoomAt('):].replace('insertRoomAt','adapter_insertRoomAt')
adapter = adapter.replace('    dungeonMap[xRoom + roomToDungeonX][yRoom + roomToDungeonY] = 1;', '    record_visit(xRoom,yRoom);\n    dungeonMap[xRoom + roomToDungeonX][yRoom + roomToDungeonY] = 1;',1)
adapter = adapter.replace('            dungeonMap[newX + roomToDungeonX][newY + roomToDungeonY] = 1;', '            record_visit(newX,newY);\n            dungeonMap[newX + roomToDungeonX][newY + roomToDungeonY] = 1;',1)
adapter = adapter.replace('    unsigned frame_count = 1;', '    unsigned frame_count = 1;\n    observe_frames(frame_count);',1).replace('            ++frame_count;', '            ++frame_count;\n            observe_frames(frame_count);',1)
prefix = r"""
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
static unsigned cases, which, counts[2], oracle_depth, oracle_peak, adapter_peak;
static short trace[2][CAP][2];
static uint64_t random_state=UINT64_C(0x78246a9cb85de17f);
static uint32_t random32(void){random_state^=random_state<<13;random_state^=random_state>>7;random_state^=random_state<<17;return (uint32_t)random_state;}
static void record_visit(short x,short y){CHECK(counts[which]<CAP);trace[which][counts[which]][0]=x;trace[which][counts[which]++][1]=y;}
static void oracle_enter(short x,short y){++oracle_depth;if(oracle_depth>oracle_peak)oracle_peak=oracle_depth;record_visit(x,y);}
static void observe_frames(unsigned n){CHECK(n<=CAP);if(n>adapter_peak)adapter_peak=n;}
struct grid {uint64_t pre;short cells[DCOLS][DROWS];uint64_t post;};
"""
suffix = r"""
static void run_case(short room[DCOLS][DROWS],short initial[DCOLS][DROWS],short dx,short dy,short sx,short sy,bool alias) {
    struct grid a={.pre=0x1741d7,.post=0x819b27},b={.pre=0x1741d7,.post=0x819b27};
    short *ar[DCOLS],*br[DCOLS],*rr[DCOLS];
    memcpy(a.cells,initial,sizeof(a.cells));memcpy(b.cells,initial,sizeof(b.cells));
    for(int x=0;x<DCOLS;++x){ar[x]=a.cells[x];br[x]=b.cells[x];rr[x]=room[x];}
    counts[0]=counts[1]=0;oracle_depth=0;which=0;uint64_t rng=random_state;
    oracle_insertRoomAt(ar,alias?ar:rr,dx,dy,sx,sy);CHECK(oracle_depth==0);CHECK(random_state==rng);
    which=1;adapter_insertRoomAt(br,alias?br:rr,dx,dy,sx,sy);CHECK(random_state==rng);
    CHECK(a.pre==0x1741d7 && a.post==0x819b27 && b.pre==0x1741d7 && b.post==0x819b27);
    CHECK(!memcmp(a.cells,b.cells,sizeof(a.cells)));CHECK(counts[0]==counts[1]);
    CHECK(!memcmp(trace[0],trace[1],counts[0]*sizeof(trace[0][0])));++cases;
}
int main(void) {
    static short room[DCOLS][DROWS],initial[DCOLS][DROWS];
    for(int x=0;x<DCOLS;++x)for(int y=0;y<DROWS;++y)room[x][y]=1;
    run_case(room,initial,0,0,0,0,false);
    CHECK(counts[0]==CAP && oracle_peak==CAP && adapter_peak==CAP);
    run_case(room,initial,0,0,DCOLS-1,DROWS-1,false);
    memset(room,0,sizeof(room));
    for(int y=0;y<DROWS;++y)for(int x=0;x<DCOLS;++x)
        room[x][y]=(y%2==0 || ((y/2)%2==0?x==DCOLS-1:x==0));
    run_case(room,initial,0,0,0,0,false);
    memset(room,0,sizeof(room));room[0][0]=room[DCOLS-1][DROWS-1]=1;
    run_case(room,initial,0,0,0,0,false);run_case(room,initial,0,0,DCOLS-1,DROWS-1,false);
    for(int x=0;x<DCOLS;++x)for(int y=0;y<DROWS;++y)room[x][y]=1;
    for(int dx=-2;dx<=2;++dx)for(int dy=-2;dy<=2;++dy){
        short sx=dx<0?-dx:0,sy=dy<0?-dy:0;
        run_case(room,initial,dx,dy,sx,sy,false);
        sx=dx>0?DCOLS-1-dx:DCOLS-1;sy=dy>0?DROWS-1-dy:DROWS-1;
        run_case(room,initial,dx,dy,sx,sy,false);
    }
    for(unsigned n=0;n<2048;++n){
        unsigned density=n%101;
        for(int x=0;x<DCOLS;++x)for(int y=0;y<DROWS;++y){room[x][y]=(random32()%100<density)?((n&1)?-2:1):0;initial[x][y]=random32()%11==0?(short)(1+random32()%5):0;}
        short dx=(short)(random32()%11)-5,dy=(short)(random32()%11)-5;
        short sx=(short)(random32()%DCOLS),sy=(short)(random32()%DROWS);
        if(sx+dx<0)sx=-dx;if(sx+dx>=DCOLS)sx=DCOLS-1-dx;
        if(sy+dy<0)sy=-dy;if(sy+dy>=DROWS)sy=DROWS-1-dy;
        run_case(room,initial,dx,dy,sx,sy,false);
        if(n%32==0)run_case(room,initial,dx,dy,sx,sy,true);
    }
    printf("{\"cases\":%u,\"map_cells\":%u,\"oracle_max_depth\":%u,\"adapter_max_frames\":%u,\"grid_and_full_visit_order_equal\":true,\"rng_state_unchanged_by_both\":true}\n",cases,CAP,oracle_peak,adapter_peak);
    return 0;
}
"""
harness=a.output/'room-stack-equivalence.c';harness.write_text(prefix+original+'\n'+adapter+'\n'+suffix)
command=[a.cc,'-std=c11','-O2','-g','-fno-optimize-sibling-calls','-fsanitize=address,undefined','-fno-omit-frame-pointer',str(harness),'-o',str(a.output/'room-stack-equivalence')]
with (a.output/'compile.log').open('w') as log:subprocess.run(command,stdout=log,stderr=subprocess.STDOUT,check=True)
env={**os.environ,'ASAN_OPTIONS':'detect_leaks=1:abort_on_error=1','UBSAN_OPTIONS':'halt_on_error=1:print_stacktrace=1'}
r=subprocess.run([str(a.output/'room-stack-equivalence')],stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True,env=env)
(a.output/'run.stdout').write_text(r.stdout);(a.output/'run.stderr').write_text(r.stderr)
assert r.returncode==0,(r.returncode,r.stderr[-3000:])
result={'passed':True,'compiler_command':command,'native_auxiliary_test_only':True,'upstream_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),'generated_round_trip_exact':True,'fail_closed_checks':negative,**json.loads(r.stdout)}
(a.output/'result.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))
