// SPDX-License-Identifier: MIT
#include "../render_pacing.hpp"
#include <cassert>
#include <cstdio>
int main(){
 ash::RenderPacing p;
 assert(p.ready(0,0,0,0,true));assert(!p.ready(1000,1,0,0,false));
 assert(p.ready(17000,1,1,0,false));assert(!p.ready(33000,2,1,0,false));
 assert(p.ready(49000,2,1,1,false));
 // A cancelled permission frame creates a permanent counter deficit.
 assert(!p.ready(100000,3,1,1,true));assert(!p.ready(199999,3,1,1,false));
 assert(p.ready(200000,3,1,1,false));
 // Do not mistake that deficit for another genuinely pending frame.
 assert(!p.ready(216000,4,1,1,false));assert(p.ready(232000,4,2,1,false));
 assert(!p.ready(248000,5,2,1,false));assert(p.ready(264000,5,3,1,false));
 // Long active-fight work must not grant an extra permanent queue slot.
 assert(!p.ready(300000,6,3,1,false));assert(!p.ready(2300000,6,3,1,false));
 assert(p.ready(2316000,6,4,1,false));
 assert(!p.ready(2400000,7,4,1,true));assert(!p.ready(0,7,4,1,true));
 assert(p.ready(100000,7,4,1,true));
 assert(p.ready(116000,7,6,1,false));assert(p.cancelled==0);
 std::puts("Pacing: completed/dropped lists, cancelled deficit, genuine pending work, long GPU work and clock reset passed.");
}
