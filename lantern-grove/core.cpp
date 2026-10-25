// SPDX-License-Identifier: CC0-1.0
#include "core.hpp"
#include <string.h>
namespace grove {
namespace {
uint32_t cap_add(uint32_t a, uint64_t b) { return b >= LIMIT-a ? LIMIT : a+uint32_t(b); }
void put32(uint8_t *p,uint32_t v) { for(unsigned i=0;i<4;++i) p[i]=uint8_t(v>>(8*i)); }
uint32_t get32(const uint8_t *p) { uint32_t v=0; for(unsigned i=0;i<4;++i) v|=uint32_t(p[i])<<(8*i); return v; }
void put64(uint8_t *p,uint64_t v) { put32(p,uint32_t(v)); put32(p+4,uint32_t(v>>32)); }
uint64_t get64(const uint8_t *p) { return get32(p)|(uint64_t(get32(p+4))<<32); }
uint32_t hash(const uint8_t *p) { uint32_t h=2166136261u; for(unsigned i=0;i<124;++i) h=(h^p[i])*16777619u; return h; }
bool valid(const State &s) {
    for(unsigned i=0;i<3;++i) if(s.resources[i]>LIMIT||s.pending[i]>capacity(s)||s.fraction[i]>=1000000) return false;
    if(s.level[0]<1||s.level[0]>5||s.level[1]>4||s.level[2]>4||s.level[3]>3||s.sound>1||s.view>3||s.selected>3) return false;
    if(s.collected>LIMIT||s.taps>LIMIT||s.cycle_ms>=120000||s.produced_ms>315360000000ull) return false;
    if(s.last_wall&&!valid_wall(s.last_wall)) return false;
    // Goals are claimed in displayed order and their monotonic conditions must
    // still hold; an invented completion mask is not a valid saved grove.
    if((unsigned(s.goals)&(unsigned(s.goals)+1))!=0) return false;
    for(unsigned g=0;g<8;++g) if((s.goals&(1u<<g))&&!achieved(s,g)) return false;
    return true;
}
}
void fresh(State &s) { s=State{}; }
bool valid_wall(uint64_t v) { return v>=MIN_UTC&&v<MAX_UTC; }
uint32_t rate(const State &s,unsigned r) {
    if(r==0) return 1000u*s.level[0]+700u*s.level[2]+300u*s.level[3];
    if(r==1) return 500u+650u*s.level[1]+100u*s.level[0];
    return r==2 ? 200u+220u*s.level[3]+80u*s.level[0]+60u*s.level[2] : 0;
}
uint32_t capacity(const State &s) { return 120u+1500u*s.level[1]+3000u*s.level[3]; }
void produce(State &s,uint64_t ms) {
    if(ms>MAX_IDLE_MS) ms=MAX_IDLE_MS;
    const uint32_t cap=capacity(s);
    for(unsigned i=0;i<3;++i) {
        const uint64_t v=ms*rate(s,i)+s.fraction[i];
        const uint64_t whole=v/1000000;
        s.pending[i]=uint32_t(whole>=cap-s.pending[i]?cap:s.pending[i]+whole);
        s.fraction[i]=s.pending[i]==cap?0:uint32_t(v%1000000);
    }
    s.cycle_ms=uint32_t((s.cycle_ms+ms)%120000);
    s.produced_ms=s.produced_ms+ms>315360000000ull?315360000000ull:s.produced_ms+ms;
}
Price price(const State &s,unsigned item) {
    Price p{}; if(item>=4) return p;
    const unsigned n=s.level[item];
    if(item==0) { p.value[0]=8*n*n; p.value[1]=4*n; p.value[2]=n>2?3*n:0; }
    if(item==1) { p.value[0]=10+12*n; p.value[2]=3+3*n; }
    if(item==2) { p.value[0]=18+80*n*n; p.value[1]=8+24*n*n; p.value[2]=4+10*n*n; }
    if(item==3) { p.value[0]=22+160*n*n; p.value[1]=12+80*n*n; p.value[2]=6+35*n*n; }
    return p;
}
bool buy(State &s,unsigned item) {
    static constexpr uint8_t maximum[4]={5,4,4,3};
    if(item>=4||s.level[item]>=maximum[item]) return false;
    Price p=price(s,item);
    for(unsigned i=0;i<3;++i) if(s.resources[i]<p.value[i]) return false;
    for(unsigned i=0;i<3;++i) s.resources[i]-=p.value[i];
    ++s.level[item]; return true;
}
bool collect(State &s) {
    uint64_t amount=0;
    for(unsigned i=0;i<3;++i) { amount+=s.pending[i]; s.resources[i]=cap_add(s.resources[i],s.pending[i]); s.pending[i]=0; }
    // Active tending adds one Lumen; automatic income remains in the basket.
    s.resources[0]=cap_add(s.resources[0],1); s.collected=cap_add(s.collected,amount+1); s.taps=cap_add(s.taps,1); return true;
}
bool achieved(const State &s,unsigned g) {
    switch(g) {
    case 0:return s.collected>=10;
    case 1:return s.level[0]>=2;
    case 2:return s.level[1]>=1;
    case 3:return s.collected>=60;
    case 4:return s.level[2]>=1;
    case 5:return s.level[3]>=1;
    case 6:return s.level[0]>=4&&s.level[1]>=2;
    case 7:return s.level[2]>=3&&s.level[3]>=2&&s.collected>=1800;
    default:return false;
    }
}
bool claim(State &s,unsigned g) {
    if(g>=8||s.goals!=uint8_t((1u<<g)-1)||!achieved(s,g)) return false;
    s.goals|=uint8_t(1u<<g);
    for(unsigned i=0;i<3;++i) s.resources[i]=cap_add(s.resources[i],(g+1)*(i==0?5u:2u));
    return true;
}
const char *goal_title(unsigned g) { static const char *v[]={"FIRST FIREFLIES","GROW A CANOPY","A LITTLE WELL","SHARED HARVEST","LIGHT THE PATH","A KEEPER'S HOME","THE DEEP GROVE","LANTERN FESTIVAL"}; return g<8?v[g]:""; }
const char *goal_detail(unsigned g) { static const char *v[]={"COLLECT 10 TOTAL","CANOPY LEVEL 2","BUILD WELL LEVEL 1","COLLECT 60 TOTAL","BUILD LANTERN LEVEL 1","BUILD CABIN LEVEL 1","CANOPY 4 + WELL 2","LANTERN 3 CABIN 2 + 1800 TOTAL"}; return g<8?v[g]:""; }
const char *upgrade_name(unsigned i) { static const char *v[]={"CANOPY","DEW WELL","LANTERN","KEEPER CABIN"}; return i<4?v[i]:""; }
const char *resource_name(unsigned i) { static const char *v[]={"LUMEN","DEW","SEED"}; return i<3?v[i]:""; }
Away offline(State &s,uint64_t now) {
    Away result{};
    if(valid_wall(now)&&valid_wall(s.last_wall)&&now>=s.last_wall) {
        uint64_t seconds=now-s.last_wall;
        if(seconds>MAX_IDLE_MS/1000) seconds=MAX_IDLE_MS/1000;
        result={uint32_t(seconds),true}; produce(s,seconds*1000);
    }
    stamp(s,now); return result;
}
void stamp(State &s,uint64_t now) {
    // Valid clock rollback cannot lower the saved high-water mark. An invalid
    // clock explicitly disarms cross-process income until a fresh valid anchor.
    if(!valid_wall(now)) s.last_wall=0;
    else if(now>s.last_wall) s.last_wall=now;
}
size_t encode(const State &s,uint8_t *p,size_t n) {
    if(!p||n<SAVE_BYTES||!valid(s)) return 0;
    memset(p,0,SAVE_BYTES); memcpy(p,"LGV1",4); p[4]=1;
    for(unsigned i=0;i<3;++i) { put32(p+8+4*i,s.resources[i]); put32(p+20+4*i,s.pending[i]); put32(p+32+4*i,s.fraction[i]); }
    memcpy(p+44,s.level,4); p[48]=s.goals;p[49]=s.sound;p[50]=s.view;p[51]=s.selected;
    put32(p+52,s.collected);put32(p+56,s.taps);put32(p+60,s.cycle_ms);put64(p+64,s.produced_ms);put64(p+72,s.last_wall);
    put32(p+124,hash(p)); return SAVE_BYTES;
}
bool decode(State &s,const uint8_t *p,size_t n) {
    if(!p||n!=SAVE_BYTES||memcmp(p,"LGV1\1\0\0\0",8)||get32(p+124)!=hash(p)) return false;
    for(unsigned i=80;i<124;++i) if(p[i]) return false;
    State t;
    for(unsigned i=0;i<3;++i) { t.resources[i]=get32(p+8+4*i); t.pending[i]=get32(p+20+4*i); t.fraction[i]=get32(p+32+4*i); }
    memcpy(t.level,p+44,4);t.goals=p[48];t.sound=p[49];t.view=p[50];t.selected=p[51];
    t.collected=get32(p+52);t.taps=get32(p+56);t.cycle_ms=get32(p+60);t.produced_ms=get64(p+64);t.last_wall=get64(p+72);
    if(!valid(t)) return false;
    s=t;return true;
}
}
