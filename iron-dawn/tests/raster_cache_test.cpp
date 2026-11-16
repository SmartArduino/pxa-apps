// SPDX-License-Identifier: MIT
#include "fx.hpp"
#include <cassert>
#include <cstdio>
#include <vector>
using namespace iron;
uint32_t fingerprint(const std::vector<uint16_t>& p,uint32_t h){for(auto c:p)h=(h^c)*16777619u;return h;}
int main(){uint32_t text_hash=2166136261u,fx_hash=text_hash;std::vector<uint16_t> p(320*160+2,0xace1);Raster r(p.data()+1,320,160,320);
 for(int size:{9,10,11,12,13,15,16,18,20,26,40})for(int pass=0;pass<3;++pass){for(unsigned i=1;i+1<p.size();++i)p[i]=i*177u; r.clip={7,4,302,151};r.text(pass==2?-30:16,pass==1?-7:9,"钢铁黎明 / 1580 +25 框选",pass?gold:cyan,size);r.text(318,90,"长弓火炮 · 0星",cream,size,2);r.ring(210,50,24,12,cyan);r.line(-15,110,330,110,gold,3);r.line(310,-6,310,170,red,2);r.shade({-3,103,330,70},navy,13);assert(p.front()==0xace1&&p.back()==0xace1);text_hash=fingerprint(p,text_hash);}
 std::vector<uint16_t> q(96*80+2,0xa55a);Raster effect(q.data()+1,96,80,96);
 for(unsigned clip=0;clip<vfx::clips.size();++clip)for(int frame=0;frame<vfx::clips[clip].frames;++frame)for(float scale:{.45f,.72f,1.f,1.3f}){for(unsigned i=1;i+1<q.size();++i)q[i]=i*89u;effect.clip={6,4,83,69};draw_fx(effect,clip,frame%2?-5:58,frame%3?44:83,(frame+.25f)/vfx::clips[clip].frames,scale);assert(q.front()==0xa55a&&q.back()==0xa55a);fx_hash=fingerprint(q,fx_hash);}
 std::printf("raster=%08x fx=%08x\n",text_hash,fx_hash);
// Captured from the released 1.2 renderer, including clipped fonts and all FX frames.
 assert(fx_hash==0x67af41b0u);
}
