// SPDX-License-Identifier: MIT
#include "../static_scene_cache.hpp"
#include <cassert>
#include <cstdio>
#include <vector>
static starsoil::State s;
static starsoil_scene::StaticSceneCache cache;
int main() {
  using namespace starsoil;
  new_game(s,4171231849u);s.clock=100000;
  std::vector<uint16_t> frame(288*288),saved;
  auto key=[&](int x=550,int y=310,uint32_t t=1000,int w=288,int h=288) {
    return starsoil_scene::BackgroundKey::make(s,w,h,x,y,t,25);
  };
  auto paint=[&] {
    for(unsigned i=0;i<frame.size();++i)frame[i]=uint16_t(i*37);
    saved=frame;cache.store(frame.data());
  };
  assert(!cache.restore(s,key(),frame.data()));paint();
  assert(!cache.restore(s,key(),frame.data()));paint();
  std::fill(frame.begin(),frame.end(),0);
  assert(cache.restore(s,key(),frame.data()));
  assert(std::equal(frame.begin()+288*25,frame.end(),saved.begin()+288*25));
  const int at=31*WorldW+42;
  for(int edit=0;edit<7;++edit) {
    switch(edit) {
      case 0:s.tiles[at]=Torch;break;
      case 1:s.tiles[at]=Air;break;
      case 2:s.walls[at]=Plank;break;
      case 3:s.heights[42]++;break;
      case 4:s.player.x+=16;break;
      case 5:s.equip[7]=GlowCharm;break;
      case 6:s.bag[s.selected].item=Torches;break;
    }
    assert(!cache.restore(s,key(),frame.data()));paint();
    if(!cache.restore(s,key(),frame.data()))paint();
    assert(cache.restore(s,key(),frame.data()));
  }
  // A trunk just above the visible bounds may change the visible crown.
  s.tiles[7*WorldW+40]=Log;
  assert(!cache.restore(s,key(),frame.data()));paint();
  assert(cache.restore(s,key(),frame.data()));
  assert(!cache.restore(s,key(551),frame.data()));paint();
  assert(!cache.restore(s,key(552),frame.data()));paint();
  assert(!cache.restore(s,key(552),frame.data()));paint();
  assert(cache.restore(s,key(552),frame.data()));
  assert(!cache.restore(s,key(552,310,1400),frame.data()));paint();
  s.clock=270000;
  for(int i=0;i<3;++i)assert(!cache.restore(s,key(),frame.data()));
  // Deep cave geometry can be reused at night; dynamic motes stay outside it.
  assert(!cache.restore(s,key(550,1200),frame.data()));paint();
  assert(!cache.restore(s,key(550,1200),frame.data()));paint();
  assert(cache.restore(s,key(550,1200),frame.data()));
  cache.invalidate();assert(!cache.restore(s,key(550,1200),frame.data()));
  puts("PASS static-scene reuse, exact camera/cloud keys, edits, tree margins, lighting, night fallback and cave reuse");
}
