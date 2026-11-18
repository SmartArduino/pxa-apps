// SPDX-License-Identifier: MIT
#include "../animation.hpp"
#include <cassert>
#include <array>
#include <cstdio>
int main(){using namespace garden;
 auto clip=garden_art::coralflower_action;assert(clip.count==12);std::array<bool,12> seen{};
 for(int i=0;i<120;++i)seen[animation_frame(clip,float(i)/120)-clip.first]=true;for(auto v:seen)assert(v);assert(animation_frame(clip,0)==clip.first);assert(animation_frame(clip,clip.seconds)==clip.first);assert(animation_frame(clip,-1)==clip.first);
 for(unsigned first=0;first<540;first+=12){auto reference=garden_art::frames[first];for(unsigned i=1;i<12;++i){auto f=garden_art::frames[first+i];assert(f.w==reference.w&&f.h==reference.h&&f.ox==reference.ox&&f.oy==reference.oy&&f.cw==reference.cw&&f.ch==reference.ch);}}
 auto f=garden_art::frames[clip.first];
 for(float zoom:{1.f,480.f/296,2.f,2.88378f}){
  auto previous=actor_bounds(f,50,100,25,zoom);const auto first=previous;
  for(int i=1;i<100;++i){auto r=actor_bounds(f,50+i*.1f,100,25,zoom);assert(r.w==first.w&&r.h==first.h&&r.y==first.y);assert(r.x>=previous.x&&r.x-previous.x<=1);previous=r;}
  auto shifted=actor_bounds(f,51,101,25,zoom);assert(shifted.w==first.w&&shifted.h==first.h);
 }
 std::puts("12 distinct playback slots, cyclic phase, stable frame size and smooth native-pixel movement passed");}
