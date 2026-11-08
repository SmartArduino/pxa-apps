// SPDX-License-Identifier: MIT
#include "../mines.hpp"
#include "../wilderness.hpp"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <queue>
using namespace farm;
static void stand(State&s,int x,int y){s.px=x*16+8;s.py=y*16+8;}
static void beside(State&s,int x,int y){for(auto d:{Point{0,1},Point{1,0},Point{0,-1},Point{-1,0}})if(walkable(s,x+d.x,y+d.y)){stand(s,x+d.x,y+d.y);return;}assert(false);}
static void sign(uint8_t*b,int bytes){uint32_t h=2166136261u;for(int i=0;i<bytes-4;++i)h=(h^b[i])*16777619u;for(int i=0;i<4;++i)b[bytes-4+i]=h>>(8*i);}
int main() {
 State s,t;uint8_t bytes[SaveBytes];fresh(s);s.story=3;
 for(int i=0;i<WildTreeCount;++i) {
  fresh(s);s.story=3;s.region=unsigned(WildTrees[i].region);s.visited|=1u<<s.region;s.tool=unsigned(Tool::Axe);
  auto p=WildTrees[i];beside(s,p.x,p.y);assert(ground(s,p.x,p.y)==Ground::Tree);
  assert(act(s,p.x,p.y)==Result::Struck&&s.wild_damage[i]==1);
  assert(encode(s,bytes,sizeof(bytes))==SaveBytes&&decode(t,bytes,sizeof(bytes))&&t.wild_damage[i]==1);
  while(wild_stage(s,i)!=3){auto expected=wild_hits(s,i)==1?Result::Cleared:Result::Struck;assert(act(s,p.x,p.y)==expected);}
  assert(s.wood==4&&walkable(s,p.x,p.y));auto energy=s.energy;assert(act(s,p.x,p.y)==Result::Blocked&&s.wood==4&&s.energy==energy);
  for(int night=0;night<7;++night)wild_night(s);assert(wild_stage(s,i)==2&&s.wild_damage[i]==0);
 }
 for(int grade=0;grade<3;++grade) {
  fresh(s);s.region=unsigned(Region::Forest);s.tool=4;s.tool_grade[4]=grade;auto p=WildTrees[1];beside(s,p.x,p.y);
  int hits=0;while(wild_stage(s,1)!=3){act(s,p.x,p.y);++hits;}assert(hits==(grade==0?5:grade==1?3:2));
 }
 fresh(s);s.region=unsigned(Region::Orchard);s.visited|=1u<<s.region;s.day=40;assert(forage_available(s,6));s.wild_age[17]=6;assert(!forage_available(s,6));
 fresh(s);s.region=unsigned(Region::Forest);stand(s,22,4);assert(act(s,22,3)==Result::Travel&&in_mine(s)&&s.sword_owned&&s.tool==7&&(s.visited&256));
 assert(!enter_mine(s,5));assert(encode(s,bytes,sizeof(bytes))&&decode(t,bytes,sizeof(bytes))&&t.mine_depth==1&&t.sword_owned);
 for(int floor=1;floor<=MineFloors;++floor) {
  load_mine_floor(s,floor);s.energy=100;s.tool=5;
  bool seen[MapH][MapW]{};std::queue<Point> q;q.push({16,4});seen[4][16]=true;
  while(!q.empty()){auto p=q.front();q.pop();for(auto d:{Point{0,1},Point{1,0},Point{0,-1},Point{-1,0}}){int x=p.x+d.x,y=p.y+d.y;if(x>=0&&y>=0&&x<MapW&&y<MapH&&!seen[y][x]&&walkable(s,x,y)){seen[y][x]=true;q.push({x,y});}}}
  assert(seen[2][16]&&seen[17][25]);
  for(int i=0;i<4;++i){auto p=cave_forage_tile(i);assert(seen[p.y][p.x]);}
  for(int i=0;i<3;++i){auto p=ore_tile(i);beside(s,p.x,p.y);int item=ore_item(s,i),before=item_count(s,item);while(ore_at(s,p.x,p.y)>=0){auto result=act(s,p.x,p.y);assert(result==Result::Struck||result==Result::Cleared);}assert(item_count(s,item)>unsigned(before));}
  assert(ladder_ready(s));
  auto p=cave_forage_tile(0);beside(s,p.x,p.y);assert(act(s,p.x,p.y)==Result::MineForaged&&cave_forage_at(s,p.x,p.y)<0);
  stand(s,16,4);assert(encode(s,bytes,sizeof(bytes))&&decode(t,bytes,sizeof(bytes))&&t.mine_depth==floor);
 }
 assert(s.mine_best==30&&s.ore_mined==90&&s.cave_gathered==30);
 // The guardian cannot be stun-locked; its reward survives visits and new days.
 load_mine_floor(s,30);stand(s,25,17);s.tool=7;s.sword_grade=2;s.mobs[5].windup=350;
 assert(act(s,25,16)==Result::Attacked&&s.mobs[5].windup==350);
 while(s.mobs[5].hp){s.attack_cooldown=0;assert(act(s,25,16)==Result::Attacked);}
 auto gold=s.gold;assert(mine_treasure(s)&&s.gold==gold+800&&!mine_treasure(s));
 leave_mine(s);assert(!in_mine(s)&&s.mine_depth==0);assert(enter_mine(s,30)&&s.mobs[5].hp==0);
 mine_new_day(s);assert(s.mine_rewards==1&&s.mine_best==30&&s.mine_cleared[29]==0&&enter_mine(s,30));
 // Damage requires a telegraph, respects invulnerability, and a dodge cancels it.
 load_mine_floor(s,1);stand(s,16,4);for(int i=0;i<MineMobs;++i){s.mobs[i].hp=0;s.mine_defeated[0]|=1u<<i;}
 auto &m=s.mobs[0];m={uint16_t(s.px+12),s.py,0,0,0,12,3};s.mine_defeated[0]&=~1;s.invulnerable=0;s.health=100;
 assert(!mine_tick(s,33)&&m.windup==400&&s.health==100);assert(!mine_tick(s,400)&&s.health<100);int hp=s.health;
 assert(!mine_tick(s,33)&&s.health==hp);m.windup=1;m.x=s.px+40;s.invulnerable=0;assert(!mine_tick(s,1)&&s.health==hp);
 m.x=s.px+12;m.windup=1;s.health=1;s.invulnerable=0;s.gold=500;auto goods=s.minerals[0];assert(mine_tick(s,1)&&s.region==unsigned(Region::Home)&&s.health==40&&s.gold==450&&s.minerals[0]==goods);
 // A sword cannot reach through a solid wall even if both endpoints are close.
 bool tested=false;
 for(int floor=1;floor<=30&&!tested;++floor)for(int y=3;y<21&&!tested;++y)for(int x=2;x<28&&!tested;++x)
  if(mine_floor(floor,x,y)&&!mine_floor(floor,x+1,y)&&mine_floor(floor,x+2,y)) {
   enter_mine(s,1);load_mine_floor(s,floor);s.px=x*16+15;s.py=y*16+8;s.tool=7;s.attack_cooldown=0;
   for(auto &enemy:s.mobs)enemy.hp=0;s.mobs[0]={uint16_t((x+2)*16),s.py,0,0,0,12,3};
   assert(act(s,x+1,y)==Result::Attacked&&s.mobs[0].hp==12);tested=true;
  }
 assert(tested);
 // Real-size v9 migration retains animals and never invents dungeon progress.
 fresh(s);s.intro=IntroScenes;s.gold=321;encode(s,bytes,sizeof(bytes));bytes[4]=9;sign(bytes,768);
 assert(decode(t,bytes,768)&&t.gold==321&&!t.sword_owned&&t.mine_best==0&&t.wild_age[0]==5);
 assert(!decode(t,bytes,SaveBytes));
 fresh(s);s.sword_owned=true;s.region=unsigned(Region::Town);s.visited|=2;s.gold=1000;change_item(s,39,5);change_item(s,38,2);sync_inventory(s);assert(forge_sword(s)&&s.sword_grade==1&&s.gold==920&&!forge_sword(s));
 fresh(s);change_item(s,41,2);sync_inventory(s);equip_item(s,41);assert(swap_slots(s,s.active_slot,55)&&s.active_slot==55);assert(encode(s,bytes,sizeof(bytes))&&decode(t,bytes,sizeof(bytes))&&t.active_slot==55&&t.minerals[3]==2);
 s.health=60;s.energy=100;change_item(s,43,1);sync_inventory(s);assert(eat(s,43)&&s.health==70);
 puts("PASS: 25 renewable wild trees; graded chopping; 30 connected ore/forage floors; ladder and lift gates; telegraphed combat, dodge, rescue, wall occlusion, guardian reward, forging, healing and v9/v10 persistence");
}
