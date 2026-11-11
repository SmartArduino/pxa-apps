// SPDX-License-Identifier: MIT
#include "../art.hpp"
#include <cassert>
#include <cstdio>
#include <vector>
int main() {
 constexpr int W=64,H=72;constexpr auto ground=art::rgb(181,151,97);
 for(int d=0;d<4;++d) {
  std::vector<uint16_t>male(W*H),girl(W*H),opposite(W*H),shadow(W*H,ground);
  art::Raster a{male.data(),W,H},b{girl.data(),W,H},c{opposite.data(),W,H},s{shadow.data(),W,H};
  art::person(a,32,50,d,0,false,0,3,false);
  art::person(b,32,50,d,0,false,0,3,false,0,0,0,false,{},true);
  int head=0;for(int y=20;y<36;++y)for(int x=18;x<46;++x)head+=male[y*W+x]!=girl[y*W+x];assert(head>=20);
  assert(!art::gait::feminine);
  art::gait::AppearanceGuard appearance(true);const auto &rig=art::gait::rig(d);assert(rig.side==bool(d&1));
  for(int frame=0;frame<=8;++frame) {
   std::fill(girl.begin(),girl.end(),0);std::fill(shadow.begin(),shadow.end(),ground);
   art::gait::draw(b,32,50,d,frame);art::gait::shadow(s,32,50,d,frame);
   int contacts=0;for(int y=1;y<H;++y)for(int x=0;x<W;++x)if(shadow[y*W+x]==art::blend(ground,art::ShadowTint,3)){assert(girl[(y-1)*W+x]);++contacts;}assert(contacts);
  }
  std::fill(girl.begin(),girl.end(),0);art::gait::draw(b,32,50,d,1);art::gait::draw(c,32,50,d,5);assert(girl!=opposite);
  for(int tool:{0,2,4,5,6,7})for(int phase:{0,64,144,210}) {
   std::fill(girl.begin(),girl.end(),0);art::tool_anim::use(b,32,50,d,0,tool,art::tool_anim::duration(tool)*(256-phase)/256,0);
   int visible=0;for(auto px:girl)visible+=px!=0;assert(visible>180);
  }
 }
 puts("PASS: independent four-view female art, clean hair silhouettes, registered boots, opposite limbs, six tool rigs and scoped appearance");
}
