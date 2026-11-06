// SPDX-License-Identifier: MIT
#include "../core.hpp"
#include "../settings.hpp"
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <vector>
using namespace starsoil;
static State s;
static void reset(int item=WoodPick) {
  new_game(s,314159,WorldSize::Small);
  std::fill_n(s.tiles,Cells,uint8_t(Air));std::fill_n(s.walls,Cells,uint8_t(0));
  std::fill_n(s.heights,WorldW,uint8_t(60));
  for(int x=1;x<s.width-1;++x)set_tile(s,x,60,Stone);
  for(auto &m:s.mobs)m={};for(auto &n:s.npcs)n={};for(auto &a:s.bag)a={};
  s.player={1608,960,0,0,100,0,1};s.player.grounded=true;s.clock=100000;s.invulnerable=60000;
  s.selected=0;s.bag[0]={uint8_t(item),uint16_t(item==Boards || item==Doors || item==Acorn?50:1)};
}
static Input input(bool use=false,int aim=0) {Input in;in.smart_cursor=true;in.use=use;in.aim_y=int8_t(aim);return in;}
static void step(Input in,int ms=20) {s.spawn_timer=0;tick(s,in,ms);}
static void hold(Input in,int ms) {for(int i=0;i<ms;i+=20)step(in,std::min(20,ms-i));}
int main() {
  reset();set_tile(s,101,58,Stone);step(input());
  assert(s.smart_cursor && s.target_ready && s.target_x==101 && s.target_y==58);
  hold(input(true),700);assert(tile(s,101,58)==Air && s.mined==1);
  // Default tool use never chooses the standing floor; DOWN deliberately digs it.
  reset();hold(input(true),700);assert(!s.target_ready && !s.mined && tile(s,100,60)==Stone);
  hold(input(true,1),550);assert(s.mined>0 && tile(s,100,60)==Air);
  reset();set_tile(s,100,57,Stone);hold(input(true,-1),600);
  assert(tile(s,100,57)==Air && s.mined==1);
  puts("PASS selects reachable high/low terrain; default floor safety and deliberate UP/DOWN digging");
  reset(CrystalPick);set_tile(s,101,59,Stone);set_tile(s,102,59,CopperOre);set_tile(s,103,59,IronOre);
  step(input(true));int x=s.target_x,y=s.target_y;
  assert(x==101 && y==59 && s.mining>0);
  // A newly exposed better candidate cannot steal partial mining progress.
  set_tile(s,101,58,GoldOre);step(input(true));assert(s.target_x==x && s.target_y==y);
  set_tile(s,101,58,Air);hold(input(true),550);
  assert(tile(s,101,59)==Air && tile(s,102,59)==Air && tile(s,103,59)==Air && s.mined==3);
  reset();set_tile(s,101,59,CrystalOre);set_tile(s,102,59,Stone);step(input());
  assert(s.target_ready && s.target_x==102 && s.target_y==59);
  hold(input(true),700);
  assert(tile(s,102,59)==Air && tile(s,101,59)==CrystalOre && s.mined==1);
  set_tile(s,101,58,Stone);step(input());assert(s.target_ready && tile(s,s.target_x,s.target_y)==Stone);
  reset();s.player.face=0;set_tile(s,99,58,Stone);step(input());assert(s.target_x==99 && s.target_y==58);
  // Explicit targets and fixed mode retain the original cursor rules.
  reset();set_tile(s,101,58,Stone);auto manual=input();manual.target_x=105;manual.target_y=59;
  step(manual);assert(!s.smart_cursor && s.target_x==105 && s.target_y==59);
  manual.target_x=-1;manual.smart_cursor=false;step(manual);assert(s.target_x==102 && s.target_y==59);
  puts("PASS stable work, automatic continuation, tool tier filtering, range-based mining through foreground, left-facing and exact/manual overrides");
  // Explicit touch, fixed cursor and smart cursor share the mining action's
  // range and tier limits, including a fully obstructed target.
  for (int mode = 0; mode < 3; ++mode) {
    reset();
    for (int row = 54; row <= 59; ++row) set_tile(s,101,row,CrystalOre);
    set_tile(s,102,59,Stone);
    s.walls[59*WorldW+102]=Plank;
    auto through = input(true);
    if (mode == 0) { through.target_x=102; through.target_y=59; }
    if (mode == 1) through.smart_cursor=false;
    hold(through,700);
    assert(tile(s,102,59)==Air && s.walls[59*WorldW+102]==Plank && s.mined==1);
    for (int row = 54; row <= 59; ++row) assert(tile(s,101,row)==CrystalOre);
  }
  reset();set_tile(s,102,59,CrystalOre);auto weak=input(true);
  weak.target_x=102;weak.target_y=59;hold(weak,1200);
  assert(tile(s,102,59)==CrystalOre && !s.mined);
  reset();set_tile(s,107,59,Stone);auto distant=input(true);
  distant.target_x=107;distant.target_y=59;hold(distant,1200);
  assert(tile(s,107,59)==Stone && !s.mined);
  hold(input(true),700);assert(!s.target_ready && !s.mined);
  puts("PASS touch/fixed/smart mining across a solid barrier; foreground/backing separation, insufficient power and out-of-range targets");
  reset(WoodAxe);set_tile(s,101,58,Leaves);set_tile(s,102,59,Log);set_tile(s,102,58,Log);set_tile(s,102,57,Leaves);
  step(input());assert(tile(s,s.target_x,s.target_y)==Log);hold(input(true),500);
  assert(tile(s,102,59)==Air && tile(s,102,58)==Air && count(s,Wood)>=2);
  reset(WoodAxe);set_tile(s,100,59,Log);step(input());assert(s.target_x==100 && s.target_y==59);
  reset(WoodAxe);set_tile(s,101,59,Stone);set_tile(s,101,58,Stone);
  set_tile(s,102,59,Log);set_tile(s,102,58,Log);
  hold(input(true),500);
  assert(tile(s,101,59)==Stone && tile(s,101,58)==Stone && tile(s,102,59)==Air);
  puts("PASS axes prefer trunks, chop complete trees and find a walk-through trunk at the player");
  reset(Boards);set_tile(s,102,59,Stone);step(input());
  assert(s.target_ready && tile(s,s.target_x,s.target_y)==Air);
  int before=count(s,Boards);step(input(true));assert(count(s,Boards)==before-1);
  assert(tile(s,100,59)==Air); // Never places a solid block inside the actor.
  reset(Boards);for(int x=101;x<s.width-1;++x)set_tile(s,x,60,Air);
  step(input());assert(s.target_ready && s.target_x==101 && s.target_y==60);
  hold(input(true),620);
  assert(tile(s,101,60)==Plank && tile(s,102,60)==Plank && count(s,Boards)<49);
  auto turn=input(true);turn.move=-1;step(turn);
  assert(s.target_ready && s.target_x<100 && s.target_y==59);
  reset(Doors);step(input());assert(s.target_ready);int door_x=s.target_x;
  step(input(true));assert(tile(s,door_x,58)==DoorTop && tile(s,door_x,59)==Door && count(s,Doors)==49);
  reset(Acorn);for(int x=1;x<s.width-1;++x)set_tile(s,x,60,Grass);step(input());
  assert(s.target_ready && s.target_y==59);step(input(true));assert(count(s,Acorn)==49);
  puts("PASS supported placement, occupied-cell rejection, extending bridges, two-cell doors and valid sapling soil");
  reset(Boards);auto walls=input(true,1);walls.down=true;hold(walls,600);
  int consumed=50-count(s,Boards),placed=0;
  for(int y=0;y<s.height;++y)for(int x=0;x<s.width;++x)placed+=s.walls[y*WorldW+x]==Plank;
  assert(consumed>1 && consumed==placed);
  // Fixed/direct placement on an existing backing wall also preserves material.
  auto exact=walls;exact.smart_cursor=false;exact.target_x=100;exact.target_y=59;s.cooldown=0;
  s.walls[59*WorldW+100]=Plank;before=count(s,Boards);hold(exact,600);assert(count(s,Boards)==before);
  reset(HunterBow);add(s,Arrows,5);auto bow=input(true);step(bow);
  assert(!s.smart_cursor && s.shots[0].life && count(s,Arrows)==4);
  puts("PASS backing walls advance without duplicate material loss; bow aiming remains independent");
  std::vector<uint8_t> bytes(SaveBytes);auto n=encode(s,bytes.data(),bytes.size());State loaded;
  assert(n && decode(loaded,bytes.data(),n) && loaded.seed==s.seed);
  bool en=false,smart=false;const uint8_t legacy[]={'P','L',1,1};
  assert(starsoil_settings::decode(legacy,4,en,smart) && en && smart);
  for(bool a:{false,true})for(bool b:{false,true}) {
    uint8_t data[8];starsoil_settings::encode(data,a,b);
    assert(starsoil_settings::decode(data,8,en,smart) && en==a && smart==b);
    data[7]=1;assert(!starsoil_settings::decode(data,8,en,smart));
  }
  puts("PASS unchanged world saves; legacy language preferences and persistent cursor setting codec");
}
