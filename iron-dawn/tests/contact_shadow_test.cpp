// SPDX-License-Identifier: MIT
#include "art.hpp"
#include <cassert>
#include <cstdio>
#include <vector>
using namespace iron;
int main(){
 for(int kind=tank;kind<=harvester;++kind)for(int pose=0;pose<16;++pose){unsigned id=art::unit_first[kind]+pose;auto f=art::frames[id];auto contact=art::contacts[id];assert(contact.bottom==f.by+f.bh-1-f.oy);assert(contact.rx>0&&contact.rx<=40&&contact.ry>=1);for(float zoom:{.72f,1.f,1.3f}){std::vector<uint16_t> pixels(160*120+2,0xa55a);Raster r(pixels.data()+1,160,120,160);r.clip={4,3,150,110};ShadowArt::base(r,id,70,80,zoom);assert(pixels.front()==0xa55a&&pixels.back()==0xa55a);}}
 for(unsigned k=0;k<8;++k){auto f=art::frames[art::buildings_first+k];auto contact=art::contacts[art::buildings_first+k];assert(contact.bottom==f.by+f.bh-1-f.oy&&contact.rx<=48&&contact.ry>0);}
 for(int kind=0;kind<2;++kind)for(unsigned facing=0;facing<8;++facing)for(int phase=0;phase<8;++phase){auto gait=CombatArt::gait(kind,facing,phase/8.f,1);for(auto leg:gait.legs)assert(std::max(leg.ankle.y,leg.toe.y)+2+leg.lift>=leg.toe.y+2);}
 std::puts("Vehicle contact shadows match all 48 sprite bases; eight building bases and 128 infantry gait contacts passed");
}
