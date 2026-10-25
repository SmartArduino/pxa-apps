// SPDX-License-Identifier: CC0-1.0
#include "../core.hpp"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <initializer_list>
using namespace elements;
static void run(State &s,unsigned n){while(n--)step(s);}
static void empty(State &s){init(s);memset(s.cells,0,sizeof(s.cells));s.ui=WORLD;}
static void put(State &s,int x,int y,unsigned m){assert(x>=0&&x<W&&y>=0&&y<H&&m<MATERIALS);s.cells[y*W+x]=uint8_t(m);}
static void fix(uint8_t *data){uint32_t h=checksum(data,SAVE_SIZE-4);for(int i=0;i<4;++i)data[SAVE_SIZE-4+i]=uint8_t(h>>(8*i));}
static void equal(const State &a,const State &b){uint8_t x[SAVE_SIZE],y[SAVE_SIZE];assert(encode(a,x,sizeof(x))==SAVE_SIZE);assert(encode(b,y,sizeof(y))==SAVE_SIZE);assert(!memcmp(x,y,SAVE_SIZE));}
int main(int argc,char **argv){
    assert(argc==2);State s,t;empty(s);
    // Sand sinks through water; granular and liquid motion conserve each material.
    for(int y=8;y<19;++y)for(int x=18;x<48;++x)put(s,x,y,y<11?SAND:WATER);
    unsigned sand=count(s,SAND),water=count(s,WATER);run(s,200);assert(count(s,SAND)==sand&&count(s,WATER)==water);assert(at(s,30,47)==SAND||at(s,30,46)==SAND);
    empty(s);put(s,35,20,STEAM);step(s);assert(at(s,35,19)==STEAM||count(s,WATER)==1);run(s,1000);assert(count(s,STEAM)+count(s,WATER)==1);
    empty(s);put(s,35,20,LAVA);put(s,35,19,WATER);step(s);assert(s.cooled==1&&count(s,STONE)==1&&count(s,STEAM)==1);
    empty(s);for(int y=27;y<31;++y)for(int x=10;x<60;++x)put(s,x,y,WOOD);put(s,35,31,FIRE);run(s,600);assert(s.burned>5&&count(s,WOOD)<195);
    empty(s);put(s,35,47,SEED);put(s,36,47,WATER);run(s,100);assert(count(s,PLANT)>=1&&count(s,SEED)==0);
    // Reach all four badges through physical materials rather than loading achievements.
    init(s);reset(s,1);for(int x=25;x<47;++x)paint(s,x,28,WATER,2);run(s,300);assert(completed(s));assert(s.badges&1u);
    reset(s,2);for(int x=11;x<62;++x)paint(s,x,32,WATER,2);for(int round=0;round<10&&progress(s)<goal(2);++round){run(s,180);for(int x=12;x<61;++x)paint(s,x,35,WATER,1);}run(s,200);assert(completed(s));assert(s.badges&2u);
    reset(s,3);stroke(s,38,12,38,42,STONE,1);run(s,420);assert(s.burned>=12);assert(completed(s));assert(s.badges&4u);
    init(t);reset(t,3);run(t,420);assert(!completed(t));
    reset(s,4);for(int y=38;y<=42;y+=2)stroke(s,24,y,48,y,WATER,0);run(s,160);assert(completed(s));assert(s.badges==15u);
    reset(s,0);assert(s.badges==15u);
    // Pixel brush, circle bounds and bounded Bresenham interpolation at every edge.
    empty(s);assert(stroke(s,0,0,71,47,SAND,0));assert(count(s,SAND)==72);assert(!paint(s,-1,0,LAVA,0));assert(!paint(s,72,48,LAVA,2));assert(!stroke(s,-100000,0,999999,1,WATER,2));
    for(int y=0;y<H;++y)for(int x=0;x<W;++x)paint(s,x,y,unsigned((x+y)%MATERIALS),2);
    s.accumulator=23;s.ui=CONFIRM;s.pending=2;s.target=3;s.confirm_yes=1;uint8_t data[SAVE_SIZE],bad[SAVE_SIZE];assert(!encode(s,data,SAVE_SIZE-1));assert(encode(s,data,sizeof(data))==SAVE_SIZE);init(t);assert(decode(t,data,sizeof(data)));equal(s,t);
    run(s,750);run(t,750);equal(s,t);
    // The former zero reserved byte43 keeps old TEL1 worlds sound-enabled.
    assert(!s.muted);s.muted=1;assert(encode(s,data,sizeof(data))==SAVE_SIZE);
    assert(data[43]==1&&decode(t,data,sizeof(data))&&t.muted);equal(s,t);
    reset(t,2);assert(t.muted);data[43]=0;fix(data);assert(decode(t,data,sizeof(data))&&!t.muted);
    assert(encode(s,data,sizeof(data))==SAVE_SIZE);assert(decode(t,data,sizeof(data)));
    // Checksum, magic/version/geometry/reserved/range/nibbles reject atomically,
    // including forged-valid checksums so validation is not merely a CRC test.
    State before=t;assert(!decode(t,data,sizeof(data)-1));equal(t,before);
    for(size_t field: {size_t(0),size_t(4),size_t(5),size_t(6),size_t(7),size_t(28),size_t(30),size_t(31),size_t(32),size_t(33),size_t(34),size_t(35),size_t(36),size_t(37),size_t(38),size_t(39),size_t(40),size_t(41),size_t(42),size_t(43),size_t(48)}){
        memcpy(bad,data,sizeof(bad));bad[field]=255;fix(bad);assert(!decode(t,bad,sizeof(bad)));equal(t,before);
    }
    memcpy(bad,data,sizeof(bad));memset(bad+8,0,4);fix(bad);assert(!decode(t,bad,sizeof(bad)));equal(t,before);
    memcpy(bad,data,sizeof(bad));bad[63]^=1;assert(!decode(t,bad,sizeof(bad)));equal(t,before);
    // Thirty-two seeded mixed worlds retain bounded memory and deterministic physics.
    for(unsigned trial=1;trial<=32;++trial){init(s);s.rng=trial;for(int y=0;y<H;++y)for(int x=0;x<W;++x)put(s,x,y,unsigned((x*13+y*31+trial)%MATERIALS));run(s,160);for(uint8_t c:s.cells)assert(c<MATERIALS);}
    char path[1024];snprintf(path,sizeof(path),"%s/native-results.json",argv[1]);FILE *f=fopen(path,"w");assert(f);fprintf(f,"{\"passed\":true,\"grid\":[72,48],\"save_bytes\":1780,\"checks\":[\"mass_conservation\",\"fall_and_flow\",\"steam_condensation\",\"lava_cooling\",\"combustion\",\"watered_seed_growth\",\"four_challenges_reachable\",\"unprotected_forest_fails\",\"reset_preserves_badges\",\"stroke_and_edge_bounds\",\"750_step_exact_save_continuation\",\"version_checksum_ranges_atomic_rejection\",\"32_mixed_world_stress\"]}\n");fclose(f);puts("PASS Tiny Elements native physics, challenges, save and bounds");
}
