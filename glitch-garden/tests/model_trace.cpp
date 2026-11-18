// SPDX-License-Identifier: MIT
#include "model.hpp"
#include <bit>
#include <iostream>
int main(){using namespace garden;uint64_t hash=1469598103934665603ull;auto add=[&](uint32_t v){hash^=v;hash*=1099511628211ull;};auto real=[&](float v){add(std::bit_cast<uint32_t>(v));};
 for(int level=0;level<12;++level)for(unsigned seed:{1u,0x41c6ce57u,0xdeadbeefu}){Game g;g.start(level,seed);
  for(int step=0;step<4800&&g.outcome==Outcome::playing;++step){if(step%7==0){g.stars=999;int r=(step/7)%5,c=(step/35)%9;if(g.plants[r*9+c].kind==Defender::none)g.place(r,c,Defender(step/7%7+1));}if(step%83==0)g.remove(step/83%5,step/415%9);g.step(.025f);
   add(g.seed);add(g.stars);add(g.kills);add(g.generated);add(g.shots_fired);add(g.waves);add(unsigned(g.outcome));real(g.elapsed);real(g.next_spawn);
   for(const auto& p:g.plants){add(unsigned(p.kind));real(p.hp);real(p.cycle);real(p.animation);real(p.flash);add(p.attacking);}
   for(const auto& e:g.enemies){add(e.active);add(unsigned(e.kind));add(e.row);add(e.jumped);real(e.x);real(e.hp);real(e.cycle);real(e.animation);real(e.jump);real(e.flash);real(e.slow);add(e.attacking);}
   for(const auto& s:g.shots){add(s.active);add(s.row);add(unsigned(s.kind));real(s.x);}
  }}
 std::cout<<hash<<"\n";
}
