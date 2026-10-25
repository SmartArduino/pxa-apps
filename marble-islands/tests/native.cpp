// SPDX-License-Identifier: CC0-1.0
#include "../core.hpp"
#include "../renderer.hpp"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
using namespace marble;
static int checks;
#define CHECK(x) do { ++checks; if(!(x)) { fprintf(stderr,"CHECK failed line %d: %s\n",__LINE__,#x);abort(); } } while(0)
static float clip(float x) { return x<-1?-1:x>1?1:x; }
static void follow(State &s,int target) {
    float tx=target%7+.5f,ty=target/7+.5f;
    for(unsigned i=0;i<1200;++i) {
        float dx=tx-s.run.x,dy=ty-s.run.y;
        if(s.run.phase==Phase::WON)return;
        if(dx*dx+dy*dy<.001f && fabsf(s.run.vx)+fabsf(s.run.vy)<.12f)return;
        float dvx=clip(dx*2.8f)*.85f,dvy=clip(dy*2.8f)*.85f;
        Input input{clip(((dvx-s.run.vx)*9+s.run.vx*2.6f)/5.4f),clip(((dvy-s.run.vy)*9+s.run.vy*2.6f)/5.4f),false};
        step(s,input,20);
        CHECK(s.run.phase!=Phase::FALLING);
        if(i%37==0) { uint8_t save[128];State resume;CHECK(load(resume,save,serialize(s,save,sizeof(save))));s=resume; }
    }
    fprintf(stderr,"waypoint timeout L%d target=%d pos=%.3f,%.3f\n",s.run.level+1,target,s.run.x,s.run.y);abort();
}
static void route(State &s,int goal) {
    int from=int(s.run.y)*7+int(s.run.x),parent[49],queue[49],n=0;for(int &p:parent)p=-1;
    parent[from]=from;queue[n++]=from;
    for(int q=0;q<n&&parent[goal]<0;++q)for(int d=0;d<4;++d) {
        int x=queue[q]%7+(d==0)-(d==1),y=queue[q]/7+(d==2)-(d==3);
        char c=cell(s.run.level,x,y);if(c==' '||c=='#'||c=='O')continue;
        int t=y*7+x;if(parent[t]>=0)continue;parent[t]=queue[q];queue[n++]=t;
    }
    CHECK(parent[goal]>=0);int path[49],length=0;
    for(int p=goal;p!=from;p=parent[p])path[length++]=p;
    for(int i=length-1;i>=0;--i)follow(s,path[i]);
}
static void reseal(uint8_t *b) { uint32_t v=2166136261u;for(int i=0;i<84;++i)v=(v^b[i])*16777619u;for(int i=0;i<4;++i)b[84+i]=uint8_t(v>>(i*8)); }
static bool equal_wire(const State &a,const State &b) { uint8_t x[100],y[100];size_t n=serialize(a,x,sizeof(x));return n==serialize(b,y,sizeof(y))&&!memcmp(x,y,n); }
int main(int argc,char **argv) {
    CHECK(argc==2);State s;init(s);
    uint32_t times[8]={};
    for(int level=0;level<LEVELS;++level) {
        CHECK(start(s,level));CHECK(!start(s,LEVELS));
        for(int y=0;y<7;++y)CHECK(strlen(levels[level].rows[y])==7);
        for(int y=0;y<7;++y)for(int x=0;x<7;++x)if(cell(level,x,y)=='C')route(s,y*7+x);
        CHECK(collected(s.run)==coin_count(level));
        for(int y=0;y<7;++y)for(int x=0;x<7;++x)if(cell(level,x,y)=='G')route(s,y*7+x);
        CHECK(s.run.phase==Phase::WON);CHECK(s.progress.medals[level]>=1);CHECK(s.progress.unlocked==(level+2>8?8:level+2));
        times[level]=s.run.elapsed_ms;printf("level %d solved, %u ms, %u coins\n",level+1,times[level],collected(s.run));
    }
    // Braking really shortens stopping distance under otherwise equal physics.
    State coast=s,brake=s;start(coast,0);coast.run.vx=1.6f;brake=coast;
    for(int i=0;i<20;++i) { step(coast,{0,0,false},20);step(brake,{0,0,true},20); }
    CHECK(brake.run.x<coast.run.x-.12f);CHECK(fabsf(brake.run.vx)<fabsf(coast.run.vx)*.1f);
    // Unrailed water edges cause a genuine fall; an early goal grants no win.
    State fall=s;start(fall,1);for(int i=0;i<200&&fall.run.phase==Phase::ROLLING;++i)step(fall,{-1,0,false},20);CHECK(fall.run.phase==Phase::FALLING);
    State early=s;start(early,0);early.run.x=5.5f;early.run.y=1.5f;step(early,{0,0,false},20);CHECK(early.run.phase==Phase::ROLLING);
    // Elastic collision reverses velocity at a bumper and solid wall never leaks.
    State bounce=s;start(bounce,4);bounce.run.x=2.5f;bounce.run.y=1.95f;bounce.run.vy=1.f;bool bounced=false;
    for(int i=0;i<80&&!bounced;++i)bounced=(step(bounce,{0,0,false},10)&BOUNCE)!=0;
    CHECK(bounced);CHECK(bounce.run.vy<0);
    State wall=s;start(wall,5);wall.run.x=3.5f;wall.run.y=3.5f;for(int i=0;i<150;++i)step(wall,{0,-1,false},20);CHECK(wall.run.phase==Phase::ROLLING);CHECK(wall.run.y>=3+RADIUS-.001f);
    // Round-trip is byte-stable and every bit flip, truncation and trailing byte is rejected atomically.
    uint8_t saved[128];size_t n=serialize(s,saved,sizeof(saved));CHECK(n==88);CHECK(serialize(s,saved,n-1)==0);
    State restored;init(restored);CHECK(load(restored,saved,n));CHECK(equal_wire(s,restored));
    for(size_t len=0;len<n;++len) { State before=restored;CHECK(!load(restored,saved,len));CHECK(equal_wire(before,restored)); }
    CHECK(!load(restored,saved,n+1));
    for(size_t byte=0;byte<n;++byte)for(unsigned bit=0;bit<8;++bit) { saved[byte]^=1<<bit;State before=restored;CHECK(!load(restored,saved,n));CHECK(equal_wire(before,restored));saved[byte]^=1<<bit; }
    const int corrupt_fields[]={5,6,7,8,16,48,52,54,56,60,64,68,72};
    for(int field:corrupt_fields) { uint8_t bad[128];memcpy(bad,saved,n);bad[field]=255;if(field==16||field==48||field==56||field==60||field==64||field==68)memset(bad+field,255,4);reseal(bad);State before=restored;CHECK(!load(restored,bad,n));CHECK(equal_wire(before,restored)); }
    // Render all actual maps, preserving a moving perspective frame as evidence.
    static uint16_t pixels[VIEW_W*VIEW_H];unsigned colors=0;
    for(int level=0;level<8;++level) {
        start(s,level);render(s.run,pixels,1200);uint16_t first=pixels[0];unsigned changed=0;for(uint16_t p:pixels)changed+=p!=first;CHECK(changed>2000);
        char path[1024];snprintf(path,sizeof(path),"%s/level-%d.ppm",argv[1],level+1);FILE *f=fopen(path,"wb");CHECK(f);fprintf(f,"P6\n144 100\n255\n");
        bool palette[65536]={};for(uint16_t p:pixels) { unsigned char rgb[3]={uint8_t((p>>11)*255/31),uint8_t(((p>>5)&63)*255/63),uint8_t((p&31)*255/31)};fwrite(rgb,1,3,f);palette[p]=true; }fclose(f);
        colors=0;for(bool p:palette)colors+=p;CHECK(colors>24);
    }
    static uint16_t baseline[VIEW_W*VIEW_H];start(s,0);render(s.run,baseline,2000);
    s.run.x+=.8f;render(s.run,pixels,2000);unsigned moved=0;for(int i=0;i<VIEW_W*VIEW_H;++i)moved+=baseline[i]!=pixels[i];CHECK(moved>6);CHECK(moved<250);
    start(s,0);s.run.coins=7;render(s.run,pixels,2000);unsigned removed=0;for(int i=0;i<VIEW_W*VIEW_H;++i)removed+=baseline[i]!=pixels[i];CHECK(removed>8);CHECK(removed<500);
    start(s,6);render(s.run,baseline,2000);s.run.elapsed_ms=3000;render(s.run,pixels,2000);unsigned gates=0;for(int i=0;i<VIEW_W*VIEW_H;++i)gates+=baseline[i]!=pixels[i];CHECK(gates>20);
    printf("PASS: %d checks; 8 physically solved levels; save fuzz and real small3dlib rendering (ball=%u pixels, gems=%u, gates=%u)\n",checks,moved,removed,gates);
}
