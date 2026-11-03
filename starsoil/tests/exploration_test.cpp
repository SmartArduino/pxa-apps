// SPDX-License-Identifier: MIT
#include "../core.hpp"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <iterator>
using namespace starsoil;
static State s;
static void reset(int floor=32) {
  new_game(s,1234567,WorldSize::Small);
  std::fill(std::begin(s.tiles),std::end(s.tiles),uint8_t(Air));
  std::fill(std::begin(s.walls),std::end(s.walls),uint8_t(0));
  std::fill(std::begin(s.heights),std::end(s.heights),uint8_t(32));
  for(int x=1;x<s.width-1;++x)set_tile(s,x,floor,Stone);
  s.player={100*16+8,float(floor*16),0,0,100,0,1};
  for(auto &m:s.mobs)m={};
  s.clock=100000;s.invulnerable=60000;
  tick(s,{},20);s.spawn_timer=0;
}
static int mobs() { return int(std::count_if(std::begin(s.mobs),std::end(s.mobs),[](auto m){return m.hp>0&&m.type!=5;})); }
static void spawning() {
  reset();
  for(int ms=0;ms<11960;ms+=40)tick(s,{},40);
  assert(mobs()==0);
  tick(s,{},40);assert(mobs()==1);
  auto &first=s.mobs[0];assert(std::fabs(first.x-s.player.x)>=18*16-8);
  for(int ms=0;ms<50000;ms+=40){tick(s,{},40);assert(mobs()<=2);}
  for(int mode=0;mode<3;++mode) {
    reset(mode==2?80:32);s.clock=mode==1?260000:100000;
    for(int i=0;i<100;++i) {
      bool alive[MaxMobs]{};for(int j=0;j<MaxMobs;++j)alive[j]=s.mobs[j].hp>0;
      s.spawn_timer=spawn_rules(s).interval;tick(s,{},40);
      assert(mobs()<=(mode==0?2:mode==1?3:4));
      for(int j=0;j<MaxMobs;++j)if(!alive[j]&&s.mobs[j].hp>0) {
        const auto &m=s.mobs[j];assert(std::fabs(m.x-s.player.x)>=18*16-8);
        for(int k=0;k<MaxMobs;++k)if(k!=j&&s.mobs[k].hp>0)
          assert(std::fabs(m.x-s.mobs[k].x)>=96 || std::fabs(m.y-s.mobs[k].y)>=64);
      }
    }
    assert(mobs()>0);
  }
  // Edge candidates switch side, rather than clamping a spawn onto the player.
  for(float x:{20.0f,float((256-1)*16-8)}) {
    reset();s.player.x=x;
    for(int i=0;i<20;++i) {
      for(auto&m:s.mobs)m={};s.spawn_timer=spawn_rules(s).interval;tick(s,{},40);
      for(auto m:s.mobs)if(m.hp>0)assert(std::fabs(m.x-s.player.x)>=18*16-8);
    }
  }
  reset();s.mobs[0]={s.player.x+300,s.player.y,0,0,280,5,1};
  for(int i=0;i<20;++i){s.spawn_timer=spawn_rules(s).interval;tick(s,{},40);assert(mobs()==0);}
  s.spawn_timer=9999;respawn(s);assert(s.spawn_timer==0);
  puts("PASS slower natural spawn interval, day/night/cave caps 2/3/4, separation, world edges, boss solitude and respawn recovery");
}
static float jump(bool held) {
  reset();float start=s.player.y,top=start;
  Input in;in.jump=true;
  for(int i=0;i<80;++i) {
    in.jump=held||i==0;s.spawn_timer=0;tick(s,in,20);top=std::min(top,s.player.y);
  }
  assert(s.player.grounded && s.player.y==start);
  return start-top;
}
static void jumping() {
  float short_h=jump(false),held_h=jump(true);
  assert(short_h>48&&short_h<72&&held_h>80&&held_h<96&&held_h>short_h+24);
  reset();
  for(int x=102;x<=112;++x)for(int y=27;y<32;++y)set_tile(s,x,y,Stone);
  Input in;in.move=1;in.jump=true;
  for(int i=0;i<70;++i){s.spawn_timer=0;tick(s,in,20);}
  assert(s.player.x>103*16 && s.player.grounded && s.player.y==27*16);
  reset();for(int x=95;x<106;++x)set_tile(s,x,29,Stone);
  in.move=0;
  for(int i=0;i<50;++i){s.spawn_timer=0;tick(s,in,20);assert(s.player.y-24>=30*16);}
  reset();s.equip[5]=CloudBoots;tick(s,in,20);
  for(int i=0;i<20;++i){s.spawn_timer=0;tick(s,{},20);}
  tick(s,in,20);assert(s.jumps==2&&s.player.vy< -280);
  tick(s,{},20);float vy=s.player.vy;tick(s,in,20);assert(s.jumps==2&&s.player.vy>vy);
  printf("PASS jump height short %.1f px / held %.1f px, five-tile ledge, ceiling collision, no held auto-bounce and cloud double jump\n",short_h,held_h);
}
static void exact_interaction() {
  reset();set_tile(s,102,31,Bed);set_tile(s,103,31,Chest);
  auto home=s.home_x;assert(!interact_at(s,101,31)&&s.home_x==home&&tile(s,103,31)==Chest);
  assert(!interact_at(s,120,31)&&tile(s,103,31)==Chest);
  assert(interact_at(s,103,31)&&tile(s,103,31)==Air&&count(s,TreasureChest)==1);
  assert(!interact_at(s,103,31)&&count(s,TreasureChest)==1&&s.home_x==home);
  assert(interact_at(s,102,31)&&s.home_x==102);
  puts("PASS exact furniture targets: no nearby fallback, reach checks, single chest reward and explicit bed action");
}
int main(){spawning();jumping();exact_interaction();}
