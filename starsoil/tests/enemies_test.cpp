// SPDX-License-Identifier: MIT
#include "../core.hpp"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <vector>
using namespace starsoil;
static State s, loaded;
static void reset() {
  new_game(s, 314159);
  std::fill_n(s.tiles, Cells, uint8_t(Air));
  std::fill_n(s.walls, Cells, uint8_t(Air));
  std::fill_n(s.heights, WorldW, uint8_t(60));
  for (int x=1; x<s.width-1; ++x) set_tile(s,x,60,Stone);
  for (auto &m:s.mobs) m={};
  for (auto &n:s.npcs) n={40*16,60*16};
  s.player={1608,960,0,0,100,0,1};
  s.player.grounded=true;
  s.clock=100000;
  s.invulnerable=60000;
}
static void step(int count=1) {
  for (int i=0;i<count;++i) { s.spawn_timer=0;tick(s,{},20); }
}
static Actor &enemy(int type, float dx=120, float dy=0) {
  s.mobs[0]={s.player.x+dx,s.player.y+dy,0,0,int16_t(enemies[type].health),uint8_t(type),0};
  s.mobs[0].grounded=!flying_enemy(type);
  return s.mobs[0];
}
static int shots() {
  return int(std::count_if(std::begin(s.hostile_shots),std::end(s.hostile_shots),
                          [](const Shot &shot){return shot.life>0;}));
}
static void barrier(int x) {
  for (int y=1;y<60;++y) set_tile(s,x,y,Stone);
}
int main() {
  reset();
  for (auto size:{WorldSize::Small,WorldSize::Medium,WorldSize::Large,WorldSize::Huge}) {
    s.width=dimensions(size).width;
    for (auto sample:{std::pair{254,FrostSlime},std::pair{190,DuneBeetle},std::pair{335,ThornHornet}}) {
      int x=sample.first*s.width/384;
      for (unsigned roll=0;roll<60;++roll)
        assert(enemy_for_environment(s,x,60,roll)==(roll%4?sample.second:DewSlime));
    }
    for (unsigned roll=0;roll<60;++roll) {
      int x=70*s.width/384;
      s.clock=260000;
      assert(enemy_for_environment(s,x,60,roll)==(roll%3?Mossling:LanternMoth));
      s.clock=100000;
      assert(enemy_for_environment(s,x,70,roll)==DuskBat);
      assert(enemy_for_environment(s,x,90,roll)==(roll%3?GemCrawler:DuskBat));
    }
  }
  for (auto sample:{std::pair{254,FrostSlime},std::pair{190,DuneBeetle},
                    std::pair{335,ThornHornet},std::pair{70,LanternMoth}}) {
    reset();s.player.x=sample.first*16+8;
    s.clock=sample.second==LanternMoth?260000:100000;
    bool found=false;
    for(int attempt=0;attempt<80;++attempt) {
      for(auto &m:s.mobs)m={};
      s.spawn_timer=spawn_rules(s).interval;tick(s,{},20);
      for(const auto &m:s.mobs)if(m.hp>0) {
        found |= m.type==sample.second;
        assert(std::fabs(m.x-s.player.x)>=18*16);
        assert(m.hp==enemies[m.type].health);
      }
    }
    assert(found);
  }
  puts("PASS biome/day/night/depth selection at all world sizes and natural spawning of all four additions");
  reset();
  for(int type=1;type<EnemyCount;++type) {
    if(type==Warden)continue;
    assert(can_spawn_enemy(s,120,59,type));
    s.walls[58*WorldW+120]=Plank;
    assert(!can_spawn_enemy(s,120,59,type));
    s.walls[58*WorldW+120]=Stone;
    assert(can_spawn_enemy(s,120,59,type));
    s.walls[58*WorldW+120]=Air;
  }
  assert(!can_spawn_enemy(s,120,59,Warden) && !can_spawn_enemy(s,120,59,EnemyCount));
  for(int type:{DuskBat,LanternMoth,ThornHornet,DuneBeetle}) {
    reset();s.clock=260000;barrier(104);
    auto &m=enemy(type,120,flying_enemy(type)?-20:0);
    for(int i=0;i<250;++i) {
      step();
      assert(m.x-enemies[type].width*.5f>=105*16-.01f);
    }
  }
  puts("PASS every enemy honors player-built backing, flight/body collision and solid barriers");
  float rises[2]{};
  for(int i=0;i<2;++i) {
    reset();auto &m=enemy(i?FrostSlime:DewSlime);
    float start=m.y,top=start;
    for(int n=0;n<120;++n){step();top=std::min(top,m.y);}
    rises[i]=start-top;
  }
  assert(rises[0]<25 && rises[1]>35);
  reset();auto &beetle=enemy(DuneBeetle,-100);
  beetle.timer=1000;step();assert(beetle.vx==0);
  beetle.timer=1240;step();assert(beetle.vx==112);
  s.player.x=beetle.x-80;step();assert(beetle.vx==112);
  reset();s.clock=260000;auto &moth=enemy(LanternMoth,120,-30);
  moth.timer=1580;step();assert(std::fabs(std::hypot(moth.vx,moth.vy)-125)<.01f);
  s.clock=100000;step();assert(moth.vx>0 && moth.vy<0);step(700);assert(moth.hp==0);
  puts("PASS high-jumping frost slime, telegraphed beetle charge with locked direction, moth dash and dawn retreat");
  reset();auto &hornet=enemy(ThornHornet,120,-30);hornet.timer=1780;
  step();assert(shots()==1);hornet.hp=0;s.invulnerable=0;
  step(80);assert(s.hp==91 && shots()==0);
  reset();auto &hidden=enemy(ThornHornet,120,-30);hidden.timer=1780;barrier(104);
  step();assert(shots()==0);
  reset();auto &archer=enemy(ThornHornet,120,-30);archer.timer=1780;
  step();assert(shots()==1);archer.hp=0;barrier(104);s.invulnerable=0;
  step(80);assert(s.hp==100 && shots()==0);
  reset();auto &full=enemy(ThornHornet,120,-30);full.timer=1780;
  for(auto &shot:s.hostile_shots)shot={500,500,0,0,1000,1,1};
  s.shots[0]={500,500,0,0,1000,1,1};
  step();assert(shots()==8 && s.shots[0].life==980);
  for(const auto &shot:s.hostile_shots)assert(shot.x==500 && shot.damage==1);
  respawn(s);assert(shots()==0);
  puts("PASS hornet aim, player damage, line of sight, swept projectile collision, separate bounded pools and respawn cleanup");
  for(int type=FrostSlime;type<EnemyCount;++type) {
    reset();enemy(type,20).hp=1;
    s.bag[0]={WoodSword,1};s.selected=0;
    int before=count(s,enemies[type].drop);
    Input in;in.use=true;tick(s,in,20);
    assert(s.kills==1 && s.mobs[0].hp==0 && count(s,enemies[type].drop)>before);
  }
  reset();enemy(ThornHornet,0,-500);step();assert(s.mobs[0].hp==0 && s.kills==0);
  enemy(Warden,0,-500);step();assert(s.mobs[0].hp>0);
  s.hostile_shots[0]={500,500,0,0,1000,1,1};
  std::vector<uint8_t> bytes(SaveBytes);
  auto n=encode(s,bytes.data(),bytes.size());
  assert(n==save_size(s.width,s.height) && decode(loaded,bytes.data(),n));
  assert(loaded.seed==s.seed && loaded.hostile_shots[0].life==0);
  puts("PASS new enemy kills and loot, distant ordinary enemy cleanup, persistent boss and existing save layout");
}
