// SPDX-License-Identifier: MIT
#include "save.hpp"
#include <cassert>
#include <cstdio>
using namespace iron;
int main(){
 for(int kind=rifle;kind<=artillery;++kind){World w;int a=w.add(kind,0,{8,8}),b=w.add(tank,1,{9,9});w.fire(a,b);unsigned expected=kind==rifle?6:kind==rocket?7:kind==tank?8:9;assert(w.sound==expected);w.sound=3;w.fire(a,b);assert(w.sound==3);w.sound=0;for(auto& s:w.shots)s.alive=true;w.fire(a,b);assert(w.sound==0);}
 {World w;int building=w.add(power,1,{10,10}),vehicle=w.add(tank,1,{12,12});w.damage(vehicle,10000);assert(w.sound==13);w.emit_sound(11);assert(w.sound==13);w.damage(building,10000);assert(w.sound==12);for(unsigned cue:{1u,3u,4u,5u,6u,11u,13u}){w.emit_sound(cue);assert(w.sound==12);}}
 World w;w.init_defense(4);assert(w.start_construction(power)&&w.sound==10);w.sound=0;
 w.construction.left=.01f;w.tick(.05f);assert(w.build_ready()&&w.sound==11);
 Save s;s.world=w;s.has_run=1;s.checksum=checksum(s);Save restored;assert(decode_save(std::as_bytes(std::span{&s,1}),restored)&&restored.world.sound==11&&sizeof(Save)<31000);
 std::puts("Four weapon cues, explosion priority, full shot admission, construction/ready cues and bounded V8 save roundtrip passed");
}
