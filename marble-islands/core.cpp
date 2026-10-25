// SPDX-License-Identifier: CC0-1.0
#include "core.hpp"
#include <math.h>
#include <string.h>
namespace marble {
const Level levels[LEVELS] = {
 {"First Ripples","Collect gold gems, then roll into the mint goal.",{"       "," ....G "," .C... "," ...C. "," .C... "," S.... ","       "},26000,0xA6D1},
 {"Ribbon Bridge","Small moves. Brake before each turn.",{"       ","   .CG ","   .   "," .C.   "," .     "," S     ","       "},18000,0xB5DA},
 {"Blue Windows","Water holes have no invisible rails.",{"       "," .C..G "," . . . "," ..C.. "," . . . "," S.C.. ","       "},29000,0x8DF5},
 {"Tidal Gate","Coral gate: wait for the mint opening.",{"       "," ...CG "," .     "," .T.C. ","     . "," SC... ","       "},30000,0xF5B2},
 {"Pebble Garden","Pink bumpers bounce. Take the clear lanes.",{"       "," ...CG "," .O.O. "," ..C.. "," .O.O. "," SC... ","       "},26000,0xC675},
 {"Windmill Walk","Dark blocks stop you; the outer loop is safe.",{"       "," .C..G "," . # . "," CT.C. "," . # . "," S.C.. ","       "},34000,0xF677},
 {"Three Tides","Three timed gates. Slow down and read the tide.",{"       ","   .CG ","   T   "," .C.C. "," T   T "," S.C.. ","       "},35000,0x9E99},
 {"Sunset Crown","One last loop. Bring every gem home.",{"       "," .C..G "," T # T "," ..OC. "," . # . "," SC.C. ","       "},37000,0xEDB8}
};
static float clamp(float x,float a,float b) { return x<a?a:x>b?b:x; }
char cell(unsigned l,int x,int y) { return l<LEVELS && x>=0 && y>=0 && x<SIZE && y<SIZE ? levels[l].rows[y][x] : ' '; }
unsigned coin_count(unsigned l) { unsigned n=0; for(int y=0;y<SIZE;++y) for(int x=0;x<SIZE;++x) n+=cell(l,x,y)=='C'; return n; }
unsigned collected(const Run &r) { unsigned n=0; for(unsigned v=r.coins;v;v>>=1) n+=v&1; return n; }
bool gate_open(const Run &r,int x,int y) { return ((r.elapsed_ms+unsigned(x*173+y*211))%4400)<2700; }
bool supported(unsigned l,float x,float y) { return isfinite(x)&&isfinite(y)&&x>=0&&y>=0&&x<SIZE&&y<SIZE&&cell(l,int(x),int(y))!=' '; }
bool start(State &s,unsigned level) {
    if(level>=LEVELS || level>=s.progress.unlocked) return false;
    s.run={}; s.run.level=uint8_t(level); s.run.phase=Phase::ROLLING;
    for(int y=0;y<SIZE;++y) for(int x=0;x<SIZE;++x) if(cell(level,x,y)=='S') { s.run.x=x+.5f; s.run.y=y+.5f; }
    return true;
}
void init(State &s) { s={}; s.progress.unlocked=1; start(s,0); }
static bool box(Run &r,float x0,float y0,float x1,float y1) {
    float nx=clamp(r.x,x0,x1),ny=clamp(r.y,y0,y1),dx=r.x-nx,dy=r.y-ny;
    float q=dx*dx+dy*dy;
    if(q>=RADIUS*RADIUS) return false;
    if(q<.000001f) { // Defensive recovery for a gate closing around the ball.
        float left=r.x-x0,right=x1-r.x,top=r.y-y0,bottom=y1-r.y;
        if(left<=right&&left<=top&&left<=bottom) { dx=-1;dy=0;r.x=x0-RADIUS; }
        else if(right<=top&&right<=bottom) { dx=1;dy=0;r.x=x1+RADIUS; }
        else if(top<=bottom) { dx=0;dy=-1;r.y=y0-RADIUS; }
        else { dx=0;dy=1;r.y=y1+RADIUS; }
    } else { float d=sqrtf(q); dx/=d;dy/=d;r.x+=dx*(RADIUS-d);r.y+=dy*(RADIUS-d); }
    float dot=r.vx*dx+r.vy*dy;
    if(dot<0) { r.vx-=1.3f*dot*dx;r.vy-=1.3f*dot*dy; }
    return true;
}
static unsigned single(State &s,Input in,unsigned ms) {
    Run &r=s.run; unsigned events=NONE;
    if(r.phase==Phase::WON) return NONE;
    if(r.phase==Phase::FALLING) { unsigned next=r.fall_ms+ms;r.fall_ms=uint16_t(next>1000?1000:next);return NONE; }
    r.elapsed_ms=r.elapsed_ms+ms>3600000?3600000:r.elapsed_ms+ms;
    float dt=ms*.001f;
    in.x=clamp(in.x,-1,1);in.y=clamp(in.y,-1,1);
    float magnitude=in.x*in.x+in.y*in.y;
    if(magnitude>1) { float inv=1/sqrtf(magnitude); in.x*=inv;in.y*=inv; }
    float damping=in.brake?18.f:2.6f;
    r.vx=(r.vx+in.x*5.4f*dt)/(1+damping*dt);
    r.vy=(r.vy+in.y*5.4f*dt)/(1+damping*dt);
    r.x+=r.vx*dt;r.y+=r.vy*dt;
    for(int y=0;y<SIZE;++y) for(int x=0;x<SIZE;++x) {
        char c=cell(r.level,x,y);
        if(c=='#') box(r,float(x),float(y),x+1.f,y+1.f);
        if(c=='T'&&!gate_open(r,x,y)) box(r,float(x),y+.40f,x+1.f,y+.60f);
        if(c=='O') {
            float dx=r.x-x-.5f,dy=r.y-y-.5f,q=dx*dx+dy*dy,rad=RADIUS+.27f;
            if(q<rad*rad) {
                float d=sqrtf(q); if(d<.0001f) { dx=1;dy=0;d=1; }
                dx/=d;dy/=d;r.x=x+.5f+dx*(rad+.002f);r.y=y+.5f+dy*(rad+.002f);
                float v=r.vx*dx+r.vy*dy;
                if(v<0) { r.vx-=1.8f*v*dx;r.vy-=1.8f*v*dy; if(v<-.4f) events|=BOUNCE; }
            }
        }
    }
    if(!supported(r.level,r.x,r.y)) { r.phase=Phase::FALLING;r.fall_ms=0;r.vx=r.vy=0;return events|FALL; }
    unsigned coin=0;
    for(int y=0;y<SIZE;++y) for(int x=0;x<SIZE;++x) {
        char c=cell(r.level,x,y);float dx=r.x-x-.5f,dy=r.y-y-.5f;
        if(c=='C') { if(!(r.coins&(1u<<coin))&&dx*dx+dy*dy<.16f) { r.coins|=uint16_t(1u<<coin);events|=COLLECT; } ++coin; }
        if(c=='G' && collected(r)==coin_count(r.level) && dx*dx+dy*dy<.14f) {
            r.phase=Phase::WON;r.vx=r.vy=0;
            uint8_t medal=r.elapsed_ms<=levels[r.level].gold_ms?3:r.elapsed_ms<=levels[r.level].gold_ms*3/2?2:1;
            if(medal>s.progress.medals[r.level]) s.progress.medals[r.level]=medal;
            uint32_t &best=s.progress.best_ms[r.level];if(!best||r.elapsed_ms<best) best=r.elapsed_ms;
            if(s.progress.unlocked<r.level+2 && r.level+1<LEVELS) s.progress.unlocked=r.level+2;
            events|=WIN;
        }
    }
    return events;
}
unsigned step(State &s,Input in,unsigned ms) { if(ms>100)ms=100;unsigned e=0;while(ms) { unsigned d=ms>10?10:ms;e|=single(s,in,d);ms-=d; } return e; }
// Explicit LE wire format; independent of struct padding and host endianness.
static constexpr size_t SAVE_BYTES=88;
static uint32_t checksum(const uint8_t *p,size_t n) { uint32_t v=2166136261u;while(n--)v=(v^*p++)*16777619u;return v; }
static void put16(uint8_t *p,uint16_t v) { p[0]=uint8_t(v);p[1]=uint8_t(v>>8); }
static void put32(uint8_t *p,uint32_t v) { put16(p,uint16_t(v));put16(p+2,uint16_t(v>>16)); }
static uint16_t get16(const uint8_t *p) { return uint16_t(p[0]|unsigned(p[1])<<8); }
static uint32_t get32(const uint8_t *p) { return uint32_t(get16(p))|uint32_t(get16(p+2))<<16; }
size_t serialize(const State &s,uint8_t *p,size_t cap) {
    if(!p||cap<SAVE_BYTES)return 0;
    memset(p,0,SAVE_BYTES);memcpy(p,"MISL",4);p[4]=1;
    p[5]=s.progress.unlocked;p[6]=s.run.level;p[7]=uint8_t(s.run.phase);
    for(unsigned i=0;i<LEVELS;++i) { p[8+i]=s.progress.medals[i];put32(p+16+i*4,s.progress.best_ms[i]); }
    put32(p+48,s.run.elapsed_ms);put16(p+52,s.run.coins);put16(p+54,s.run.fall_ms);
    float values[4]={s.run.x,s.run.y,s.run.vx,s.run.vy};
    for(unsigned i=0;i<4;++i)put32(p+56+i*4,uint32_t(int32_t(values[i]*65536.f)));
    put32(p+84,checksum(p,84));return SAVE_BYTES;
}
bool load(State &s,const uint8_t *p,size_t n) {
    if(!p||n!=SAVE_BYTES||memcmp(p,"MISL",4)||p[4]!=1||get32(p+84)!=checksum(p,84))return false;
    State t{};t.progress.unlocked=p[5];t.run.level=p[6];t.run.phase=Phase(p[7]);
    if(t.progress.unlocked<1||t.progress.unlocked>LEVELS||t.run.level>=t.progress.unlocked||p[7]>2)return false;
    for(unsigned i=0;i<LEVELS;++i) {
        t.progress.medals[i]=p[8+i];t.progress.best_ms[i]=get32(p+16+i*4);
        if(p[8+i]>3||(p[8+i]==0)!=(t.progress.best_ms[i]==0)||t.progress.best_ms[i]>3600000)return false;
        if(t.progress.best_ms[i]) { uint32_t best=t.progress.best_ms[i],par=levels[i].gold_ms;unsigned medal=best<=par?3:best<=par*3/2?2:1;if(p[8+i]!=medal)return false; }
        if(i>=t.progress.unlocked&&(p[8+i]||t.progress.best_ms[i]))return false;
        if(i+1<t.progress.unlocked&&!p[8+i])return false;
    }
    for(unsigned i=72;i<84;++i)if(p[i])return false;
    t.run.elapsed_ms=get32(p+48);t.run.coins=get16(p+52);t.run.fall_ms=get16(p+54);
    if(t.run.elapsed_ms>3600000||t.run.fall_ms>1000||t.run.coins>=(1u<<coin_count(t.run.level)))return false;
    float *values[4]={&t.run.x,&t.run.y,&t.run.vx,&t.run.vy};
    for(unsigned i=0;i<4;++i)*values[i]=int32_t(get32(p+56+i*4))/65536.f;
    if(t.run.x<-.1f||t.run.x>SIZE+.1f||t.run.y<-.1f||t.run.y>SIZE+.1f||fabsf(t.run.vx)>2.2f||fabsf(t.run.vy)>2.2f)return false;
    if(t.run.phase!=Phase::FALLING&&(!supported(t.run.level,t.run.x,t.run.y)||t.run.fall_ms))return false;
    if(t.run.phase!=Phase::FALLING) {
        for(int y=0;y<SIZE;++y)for(int x=0;x<SIZE;++x) {
            char c=cell(t.run.level,x,y);float dx=t.run.x-x-.5f,dy=t.run.y-y-.5f;
            if(c=='#') { float nx=clamp(t.run.x,float(x),float(x+1)),ny=clamp(t.run.y,float(y),float(y+1));float ox=t.run.x-nx,oy=t.run.y-ny;if(ox*ox+oy*oy<(RADIUS-.003f)*(RADIUS-.003f))return false; }
            if(c=='O'&&dx*dx+dy*dy<(RADIUS+.268f)*(RADIUS+.268f))return false;
        }
    }
    if(t.run.phase==Phase::WON) {
        if(collected(t.run)!=coin_count(t.run.level)||!t.progress.medals[t.run.level]||cell(t.run.level,int(t.run.x),int(t.run.y))!='G'||t.run.vx!=0||t.run.vy!=0)return false;
    }
    s=t;return true;
}
}
