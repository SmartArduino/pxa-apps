// SPDX-License-Identifier: CC0-1.0
#include "core.hpp"
#include <string.h>
namespace elements {
namespace {
bool inside(int x,int y) {return x>=0&&x<W&&y>=0&&y<H;}
uint32_t random(State &s) {uint32_t x=s.rng;x^=x<<13;x^=x>>17;x^=x<<5;return s.rng=x;}
void put(State &s,int x,int y,uint8_t m) {if(inside(x,y))s.cells[y*W+x]=m;}
void box(State &s,int x,int y,int w,int h,uint8_t m) {for(int j=y;j<y+h;++j)for(int i=x;i<x+w;++i)put(s,i,j,m);}
void add(uint32_t &value) {if(value!=UINT32_MAX)++value;}
int neighbor(const State &s,int x,int y,unsigned m,int direction) {
    const int dx[4]={0,direction,-direction,0},dy[4]={-1,0,0,1};
    for(int n=0;n<4;++n)if(inside(x+dx[n],y+dy[n])&&at(s,x+dx[n],y+dy[n])==m)return (y+dy[n])*W+x+dx[n];
    return -1;
}
bool move(State &s,uint8_t *done,int a,int x,int y,bool through_water=false) {
    if(!inside(x,y))return false;
    const int b=y*W+x;
    const unsigned target=s.cells[b];
    if(done[b]||(target!=EMPTY&&target!=STEAM&&!(through_water&&target==WATER)))return false;
    const uint8_t m=s.cells[a];s.cells[a]=s.cells[b];s.cells[b]=m;done[a]=done[b]=1;return true;
}
void u32(uint8_t *d,uint32_t v) {for(int i=0;i<4;++i)d[i]=uint8_t(v>>(8*i));}
uint32_t r32(const uint8_t *d) {return uint32_t(d[0])|(uint32_t(d[1])<<8)|(uint32_t(d[2])<<16)|(uint32_t(d[3])<<24);}
}
const char *name(unsigned m) {static const char *const names[]={"ERASE","SAND","WATER","STONE","WOOD","FIRE","STEAM","PLANT","SEED","LAVA"};return m<MATERIALS?names[m]:"?";}
const char *scene_name(unsigned n) {static const char *const names[]={"FREE GARDEN","FILL THE JAR","GROW A FOREST","FIREBREAK","COOL THE CRUCIBLE"};return n<5?names[n]:"?";}
const char *objective(unsigned n) {static const char *const names[]={"DRAW, MIX, DISCOVER","60 WATER IN THE JAR","GROW 16 PLANT CELLS","SAVE 80 WOOD FOR 20S","COOL 60 LAVA WITH WATER"};return n<5?names[n]:"?";}
uint8_t at(const State &s,int x,int y) {return inside(x,y)?s.cells[y*W+x]:uint8_t(STONE);}
bool allowed(unsigned scene,unsigned m) {
    if(m>=MATERIALS)return false;
    if(scene==2)return m==EMPTY||m==WATER||m==SEED||m==STONE;
    if(scene==3||scene==4)return m==EMPTY||m==WATER||m==STONE;
    return scene<5;
}
void reset(State &s,unsigned scene) {
    if(scene>=5)return;
    memset(s.cells,0,sizeof(s.cells));s.scene=uint8_t(scene);s.scene_start=s.steps;s.burned=s.cooled=0;s.accumulator=0;
    s.paused=scene!=0;s.ui=WORLD;s.pending=0;s.target=uint8_t(scene);s.cx=36;s.cy=18;s.selected=uint8_t(scene);s.confirm_yes=0;
    s.material=scene==2||scene==4||scene==1?WATER:scene==3?STONE:SAND;s.page=s.material/5;
    box(s,0,45,W,3,STONE);
    if(scene==0) {
        box(s,7,36,19,9,SAND);box(s,33,40,19,5,WATER);box(s,31,39,2,6,STONE);box(s,52,39,2,6,STONE);
        box(s,60,32,2,13,WOOD);box(s,56,29,10,4,PLANT);box(s,58,26,6,3,PLANT);
    } else if(scene==1) {
        box(s,20,19,2,26,STONE);box(s,50,19,2,26,STONE);box(s,20,43,32,2,STONE);
    } else if(scene==2) {
        box(s,9,43,54,2,STONE);for(int x=18;x<=54;x+=12)put(s,x,42,SEED);
    } else if(scene==3) {
        box(s,0,43,W,2,STONE);box(s,8,31,1,12,STONE);
        box(s,15,38,41,5,WOOD);box(s,51,23,10,20,WOOD);
        box(s,9,38,6,5,LAVA);box(s,15,39,2,4,FIRE);
    } else {
        box(s,18,20,2,25,STONE);box(s,55,20,2,25,STONE);box(s,18,43,39,2,STONE);box(s,24,37,25,6,LAVA);
    }
}
void init(State &s) {memset(&s,0,sizeof(s));s.rng=0x74e19b35u;s.brush=1;reset(s,0);s.ui=HELP;}
bool paint(State &s,int x,int y,unsigned m,unsigned brush) {
    if(!inside(x,y)||!allowed(s.scene,m)||brush>2)return false;
    const int radius=brush==2?3:int(brush);bool changed=false;
    for(int dy=-radius;dy<=radius;++dy)for(int dx=-radius;dx<=radius;++dx)if(dx*dx+dy*dy<=radius*radius&&inside(x+dx,y+dy)) {
        uint8_t &cell=s.cells[(y+dy)*W+x+dx];if(cell!=m){cell=uint8_t(m);changed=true;}
    }
    return changed;
}
bool stroke(State &s,int x0,int y0,int x1,int y1,unsigned m,unsigned brush) {
    if(!inside(x0,y0)||!inside(x1,y1)||brush>2)return false;
    int dx=x1>x0?x1-x0:x0-x1,dy=y1>y0?y1-y0:y0-y1,sx=x0<x1?1:-1,sy=y0<y1?1:-1,err=dx-dy;bool changed=false;
    for(int n=0;n<W+H;++n) {changed=paint(s,x0,y0,m,brush)||changed;if(x0==x1&&y0==y1)break;const int twice=2*err;if(twice>-dy){err-=dy;x0+=sx;}if(twice<dx){err+=dx;y0+=sy;}}
    return changed;
}
unsigned count(const State &s,unsigned m) {unsigned n=0;for(uint8_t c:s.cells)n+=c==m;return n;}
unsigned goal(unsigned scene) {static const unsigned values[]={0,60,16,400,60};return scene<5?values[scene]:0;}
unsigned progress(const State &s) {
    if(s.scene==1) {unsigned n=0;for(int y=39;y<43;++y)for(int x=22;x<50;++x)n+=at(s,x,y)==WATER;return n;}
    if(s.scene==2)return count(s,PLANT);
    if(s.scene==3) {unsigned n=0;for(int y=23;y<43;++y)for(int x=51;x<61;++x)n+=at(s,x,y)==WOOD;return n>=80&&s.burned>=12?s.steps-s.scene_start:0;}
    return s.scene==4?s.cooled:0;
}
bool completed(const State &s) {return s.scene!=0&&progress(s)>=goal(s.scene);}
void step(State &s) {
    uint8_t done[CELLS]={};const int dir=(s.steps&1u)?1:-1;
    for(int y=H-1;y>=0;--y)for(int n=0;n<W;++n) {
        const int x=dir>0?n:W-1-n,i=y*W+x;const unsigned m=s.cells[i];
        if(done[i]||m==EMPTY||m==STONE)continue;
        const uint32_t r=random(s);const int side=(r&1u)?1:-1;
        if(m==WOOD||m==PLANT||m==SEED) {
            if((neighbor(s,x,y,FIRE,side)>=0||neighbor(s,x,y,LAVA,side)>=0)&&(r%4u)==0) {s.cells[i]=FIRE;done[i]=1;add(s.burned);continue;}
            if(m==SEED) {const int water=neighbor(s,x,y,WATER,side);if(water>=0&&r%3u==0){s.cells[i]=PLANT;s.cells[water]=EMPTY;done[i]=done[water]=1;continue;}}
            if(m==PLANT&&y>12&&r%12u==0) {
                const int water=neighbor(s,x,y,WATER,side);
                const unsigned above=at(s,x,y-1);
                if(water>=0&&(above==EMPTY||above==WATER)) {
                    // Consume water before forming a tip, including submerged tips.
                    s.cells[water]=EMPTY;put(s,x,y-1,PLANT);done[i-W]=done[water]=1;
                }
            }
            if(m!=SEED)continue;
        }
        if(m==LAVA) {
            const int water=neighbor(s,x,y,WATER,side);
            if(water>=0){s.cells[i]=STONE;s.cells[water]=STEAM;done[i]=done[water]=1;add(s.cooled);continue;}
            if(r%3u!=0)continue;
        }
        if(m==WATER) {
            const int fire=neighbor(s,x,y,FIRE,side);if(fire>=0){s.cells[i]=STEAM;s.cells[fire]=EMPTY;done[i]=done[fire]=1;continue;}
        }
        if(m==FIRE) {
            if(r%35u==0){s.cells[i]=EMPTY;done[i]=1;continue;}
            // The visible flame rises only one cell per fixed step.
            if(r%5u==0)move(s,done,i,x,y-1);
            continue;
        }
        if(m==STEAM) {
            if(neighbor(s,x,y,LAVA,side)<0&&neighbor(s,x,y,FIRE,side)<0&&r%80u==0){s.cells[i]=WATER;done[i]=1;continue;}
            if(!move(s,done,i,x,y-1))move(s,done,i,x+side,y);
            continue;
        }
        if(m==SAND||m==SEED||m==WATER||m==LAVA) {
            if(move(s,done,i,x,y+1,m==SAND))continue;
            if(move(s,done,i,x+side,y+1,m==SAND)||move(s,done,i,x-side,y+1,m==SAND))continue;
            if(m==WATER||m==LAVA)move(s,done,i,x+side,y);
        }
    }
    ++s.steps;
    if(completed(s))s.badges|=uint8_t(1u<<(s.scene-1));
}
uint32_t checksum(const uint8_t *data,size_t size) {uint32_t h=2166136261u;for(size_t i=0;i<size;++i)h=(h^data[i])*16777619u;return h;}
size_t encode(const State &s,uint8_t *out,size_t capacity) {
    if(!out||capacity<SAVE_SIZE)return 0;
    memset(out,0,SAVE_SIZE);memcpy(out,"TEL1",4);out[4]=1;out[5]=W;out[6]=H;
    u32(out+8,s.rng);u32(out+12,s.steps);u32(out+16,s.scene_start);u32(out+20,s.burned);u32(out+24,s.cooled);
    out[28]=uint8_t(s.accumulator);out[29]=uint8_t(s.accumulator>>8);
    const uint8_t fields[]={s.scene,s.badges,s.material,s.brush,s.paused,s.ui,s.pending,s.target,s.cx,s.cy,s.selected,s.page,s.confirm_yes,s.muted};
    memcpy(out+30,fields,sizeof(fields));
    for(int i=0;i<CELLS;i+=2)out[48+i/2]=uint8_t(s.cells[i]|(s.cells[i+1]<<4));
    u32(out+SAVE_SIZE-4,checksum(out,SAVE_SIZE-4));return SAVE_SIZE;
}
bool decode(State &s,const uint8_t *data,size_t size) {
    if(!data||size!=SAVE_SIZE||memcmp(data,"TEL1",4)||data[4]!=1||data[5]!=W||data[6]!=H||data[7]!=0||r32(data+SAVE_SIZE-4)!=checksum(data,SAVE_SIZE-4))return false;
    if(data[43]>1)return false;
    for(int i=44;i<48;++i)if(data[i])return false;
    State candidate={};candidate.rng=r32(data+8);candidate.steps=r32(data+12);candidate.scene_start=r32(data+16);candidate.burned=r32(data+20);candidate.cooled=r32(data+24);candidate.accumulator=uint16_t(data[28]|(data[29]<<8));
    candidate.scene=data[30];candidate.badges=data[31];candidate.material=data[32];candidate.brush=data[33];candidate.paused=data[34];candidate.ui=data[35];candidate.pending=data[36];candidate.target=data[37];candidate.cx=data[38];candidate.cy=data[39];candidate.selected=data[40];candidate.page=data[41];candidate.confirm_yes=data[42];candidate.muted=data[43];
    if(!candidate.rng||candidate.accumulator>=50||candidate.scene>=5||(candidate.badges&0xf0u)||candidate.material>=MATERIALS||candidate.brush>2||candidate.paused>1||candidate.ui>CONFIRM||candidate.pending>2||candidate.target>=5||candidate.cx>=W||candidate.cy>=H||candidate.selected>=5||candidate.page>1||candidate.confirm_yes>1||(candidate.ui==CONFIRM)!=(candidate.pending!=0))return false;
    for(int i=0;i<CELLS;i+=2){candidate.cells[i]=data[48+i/2]&15u;candidate.cells[i+1]=data[48+i/2]>>4;if(candidate.cells[i]>=MATERIALS||candidate.cells[i+1]>=MATERIALS)return false;}
    s=candidate;return true;
}
}
