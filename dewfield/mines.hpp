// SPDX-License-Identifier: MIT
#pragma once
#include "living.hpp"
#include "mine_layouts.hpp"
#include <algorithm>
#include <cstdlib>
namespace farm {
inline bool in_mine(const State&s){return s.region==unsigned(Region::Mine);}
inline constexpr uint32_t MineLadder=1u<<29;
inline uint32_t mine_hash(unsigned seed,unsigned x,unsigned y) {uint32_t h=seed^(x*374761393u)^(y*668265263u);h=(h^(h>>13))*1274126177u;return h^(h>>16);}
inline const char *mineral_name(int item,bool en=false) {
 const char *cn[]={"煤炭","铜矿","铁矿","金矿","晶石","洞菇","荧根菜","黏液"};
 const char *eng[]={"Coal","Copper ore","Iron ore","Gold ore","Crystal","Cave mushroom","Glowroot","Slime gel"};
 return (en?eng:cn)[std::clamp(item-38,0,7)];
}
inline const char *mine_theme(int depth,bool en=false){return depth<=10?(en?"Amber tunnels":"琥珀矿道"):depth<=20?(en?"Blue iron halls":"青铁回廊"):(en?"Crystal depths":"幽晶深窟");}
inline bool mine_floor(int depth,int x,int y) {
 if(depth<1||depth>MineFloors||unsigned(x)>=32||unsigned(y)>=24)return false;
 return bool(MineFloorRows[depth-1][y]&(1u<<x));
}
inline Point ore_tile(int i){constexpr int dx[]={-2,2,-1,2},dy[]={-1,1,2,-2};return {7+(i/4%3)*9+dx[i%4],(i/12?16:7)+dy[i%4]};}
inline int ore_at(const State&s,int x,int y) {
 if(!in_mine(s)||!s.mine_depth)return -1;
 int col=x<12?0:x<21?1:2,row=y<12?0:1,dx=x-(7+col*9),dy=y-(row?16:7);
 int k=dx==-2&&dy==-1?0:dx==2&&dy==1?1:dx==-1&&dy==2?2:dx==2&&dy==-2?3:-1;
 if(k<0)return -1;int i=row*12+col*4+k;
 return s.mine_cleared[s.mine_depth-1]&(1u<<i)?-1:i;
}
inline int ore_item(const State&s,int i) {
 unsigned h=mine_hash(s.mine_seed+s.mine_depth,i,91);
 if(h%7==0)return 38;
 if(s.mine_depth>=5&&h%11==0)return 42;
 return s.mine_depth<=10?39:s.mine_depth<=20?40:41;
}
inline int ore_hp(const State&s,int i){int item=ore_item(s,i);return item==38?3:item==39?4:item==40?5:7;}
inline int ore_hits(const State&s,int i){int power=1+s.tool_grade[5];return (ore_hp(s,i)-s.ore_damage[i]+power-1)/power;}
inline Point cave_forage_tile(int i){constexpr Point p[]={{5,8},{23,8},{5,17},{23,17}};return p[i&3];}
inline int cave_forage_item(const State&s,int i){return ((s.mine_depth+i)%3==0)?44:43;}
inline int cave_forage_at(const State&s,int x,int y) {
 if(!in_mine(s)||!s.mine_depth)return -1;
 for(int i=0;i<4;++i){auto p=cave_forage_tile(i);if(p.x==x&&p.y==y&&!(s.mine_cleared[s.mine_depth-1]&(1u<<(24+i))))return i;}return -1;
}
inline Ground mine_ground(const State&s,int x,int y) {
 if(!mine_floor(s.mine_depth,x,y))return Ground::Wall;
 return ore_at(s,x,y)>=0?Ground::Rock:Ground::Path;
}
inline bool mine_walkable(const State&s,int px,int py){return mine_ground(s,px/16,py/16)==Ground::Path;}
inline int mob_type(const State&s,int i){return s.mine_depth==30&&i==5?3:int((s.mine_depth/5+i)%3);}
inline int mob_max_hp(const State&s,int i){int type=mob_type(s,i);return type==3?90:(type==0?12:type==1?10:20)+(s.mine_depth/10)*6;}
inline Point mob_spawn(int i){return {7+(i%3)*9,(i/3?16:7)};}
inline bool mine_line(const State&s,int ax,int ay,int bx,int by) {
 int steps=std::max(std::abs(ax-bx),std::abs(ay-by))/4+1;
 for(int n=1;n<steps;++n)if(!mine_walkable(s,ax+(bx-ax)*n/steps,ay+(by-ay)*n/steps))return false;
 return true;
}
inline bool ladder_ready(const State&s){return s.mine_depth&&bool(s.mine_cleared[s.mine_depth-1]&MineLadder);}
inline void check_ladder(State&s) {
 if(__builtin_popcount(s.mine_cleared[s.mine_depth-1]&0xffffffu)>=3||__builtin_popcount(s.mine_defeated[s.mine_depth-1])>=2)
  s.mine_cleared[s.mine_depth-1]|=MineLadder;
}
inline void load_mine_floor(State&s,int floor) {
 s.mine_depth=uint8_t(floor);s.mine_loaded=uint8_t(floor);s.mine_best=std::max(int(s.mine_best),floor);
 std::fill(s.ore_damage,s.ore_damage+MineRocks,uint8_t(0));
 for(int i=0;i<MineMobs;++i) {
  auto p=mob_spawn(i);s.mobs[i]={uint16_t(p.x*16+8),uint16_t(p.y*16+8),uint16_t(400+i*100),0,0,uint8_t((s.mine_defeated[floor-1]&(1u<<i))?0:mob_max_hp(s,i)),2};
 }
 s.px=16*16+8;s.py=4*16+8;s.attack_cooldown=0;s.invulnerable=1200;
}
inline bool enter_mine(State&s,int floor=1) {
 if(floor<1||floor>MineFloors||(floor!=1&&(floor%5||floor>s.mine_best)))return false;
 bool first=!s.sword_owned;s.region=unsigned(Region::Mine);s.visited|=1u<<unsigned(Region::Mine);
 if(first){s.sword_owned=true;sync_inventory(s);equip_item(s,37);}
 load_mine_floor(s,floor);return true;
}
inline void leave_mine(State&s) {
 s.region=unsigned(Region::Forest);s.visited|=1u<<unsigned(Region::Forest);s.px=22*16+8;s.py=4*16+8;
 s.mine_depth=s.mine_loaded=0;s.attack_cooldown=s.invulnerable=0;
}
inline bool descend_mine(State&s) {
 if(!in_mine(s)||s.mine_depth>=MineFloors||!ladder_ready(s))return false;
 load_mine_floor(s,s.mine_depth+1);return true;
}
inline void mine_new_day(State&s) {
 std::fill(s.mine_cleared,s.mine_cleared+MineFloors,0u);std::fill(s.mine_defeated,s.mine_defeated+MineFloors,uint8_t(0));
 std::fill(s.ore_damage,s.ore_damage+MineRocks,uint8_t(0));for(auto &m:s.mobs)m=MineMob{};
 s.mine_depth=s.mine_loaded=0;s.invulnerable=s.attack_cooldown=0;s.mine_day=s.day;
 if(in_mine(s)){s.region=unsigned(Region::Home);s.visited|=1u<<unsigned(Region::Home);s.px=16*16+8;s.py=17*16+8;}
}
inline int mob_at(const State&s,int x,int y) {
 if(!in_mine(s))return -1;
 for(int i=0;i<MineMobs;++i)if(s.mobs[i].hp&&s.mobs[i].x/16==x&&s.mobs[i].y/16==y)return i;return -1;
}
inline Result mine_strike(State&s,int x,int y) {
 if(s.attack_cooldown)return Result::Cooldown;
 s.attack_cooldown=220;int dx=x-int(s.px)/16,dy=y-int(s.py)/16;
 if(std::abs(dx)>std::abs(dy)){dx=dx>0?1:-1;dy=0;}else{dy=dy>0?1:-1;dx=0;}
 for(int i=0;i<MineMobs;++i) {
  auto &m=s.mobs[i];if(!m.hp)continue;int mx=int(m.x)-s.px,my=int(m.y)-s.py;
  int along=mx*dx+my*dy,across=mx*dy-my*dx;
  if(along< -5||along>29||std::abs(across)>17||!mine_line(s,s.px,s.py,m.x,m.y))continue;
  int damage=8+s.sword_grade*5;m.hp=std::max(0,int(m.hp)-damage);if(mob_type(s,i)!=3){m.windup=0;m.cooldown=500;}
  if(!m.hp) {
   s.mine_defeated[s.mine_depth-1]|=1u<<i;s.monsters_slain=std::min(65535,int(s.monsters_slain)+1);
   int item=mob_type(s,i)==2?38:mob_type(s,i)==3?42:45;
   if(item_count(s,item)<999)change_item(s,item,1);
  } else if(mob_type(s,i)!=3) {
   for(int step=0;step<4;++step){int nx=int(m.x)+dx,ny=int(m.y)+dy;if(!mine_walkable(s,nx,ny))break;m.x=nx;m.y=ny;}
  }
 }
 check_ladder(s);sync_inventory(s);return Result::Attacked;
}
inline Result mine_act(State&s,int x,int y) {
 if(x==16&&y==2)return Result::MineLift;
 if(x==25&&y==17&&!(Tool(s.tool)==Tool::Sword&&mob_at(s,x,y)>=0))return s.mine_depth==30?Result::MineChest:ladder_ready(s)?Result::MineStairs:Result::NotReady;
 int forage=cave_forage_at(s,x,y);
 if(forage>=0) {
  if(!change_item(s,cave_forage_item(s,forage),1))return Result::Blocked;
  s.mine_cleared[s.mine_depth-1]|=1u<<(24+forage);s.cave_gathered=std::min(65535,int(s.cave_gathered)+1);sync_inventory(s);return Result::MineForaged;
 }
 if(Tool(s.tool)==Tool::Sword)return mine_strike(s,x,y);
 int rock=ore_at(s,x,y);
 if(rock>=0) {
  if(Tool(s.tool)!=Tool::Pick)return Result::Blocked;
  int power=1+s.tool_grade[5],item=ore_item(s,rock),count=item==42?1:2;
  bool final=s.ore_damage[rock]+power>=ore_hp(s,rock);
  if(final&&item_count(s,item)>unsigned(999-count))return Result::Blocked;
  if(!spend_energy(s,s.tool_grade[5]?2:3))return Result::NoEnergy;
  if(!final){s.ore_damage[rock]+=power;return Result::Struck;}
  s.ore_damage[rock]=0;s.mine_cleared[s.mine_depth-1]|=1u<<rock;s.ore_mined=std::min(65535,int(s.ore_mined)+1);change_item(s,item,count);check_ladder(s);sync_inventory(s);return Result::Cleared;
 }
 return mob_at(s,x,y)>=0?Result::NeedSword:Result::Blocked;
}
inline bool mine_treasure(State&s) {
 if(!in_mine(s)||s.mine_depth!=30||s.mobs[5].hp||(s.mine_rewards&1)||!near(s,25,17))return false;
 State prepared=s;
 if(!change_item(prepared,42,3)||!change_item(prepared,44,3))return false;
 prepared.gold=std::min(999999u,prepared.gold+800);prepared.mine_rewards|=1;s=prepared;sync_inventory(s);return true;
}
inline bool forge_sword(State&s) {
 if(!s.sword_owned||s.sword_grade>=2||(!in_mine(s)&&s.region!=unsigned(Region::Town)))return false;
 int ore=s.sword_grade?40:39,count=s.sword_grade?8:5,cost=s.sword_grade?180:80;
 if(item_count(s,ore)<unsigned(count)||item_count(s,38)<2||s.gold<unsigned(cost))return false;
 change_item(s,ore,-count);change_item(s,38,-2);s.gold-=cost;++s.sword_grade;sync_inventory(s);return true;
}
// Returns true only when the player is rescued. Six actors use bounded greedy
// movement; no full-map path search, floating-point physics or particle heaps.
inline bool mine_tick(State&s,uint32_t elapsed) {
 if(!in_mine(s)||!s.mine_depth)return false;
 s.invulnerable=s.invulnerable>elapsed?s.invulnerable-elapsed:0;
 s.attack_cooldown=s.attack_cooldown>elapsed?s.attack_cooldown-elapsed:0;
 for(int i=0;i<MineMobs;++i) {
  auto &m=s.mobs[i];if(!m.hp)continue;int dx=int(s.px)-m.x,dy=int(s.py)-m.y,dist=std::abs(dx)+std::abs(dy);
  if(m.windup) {
   m.windup=m.windup>elapsed?m.windup-elapsed:0;
   if(!m.windup) {
    if(dist<25&&!s.invulnerable&&mine_line(s,m.x,m.y,s.px,s.py)) {
     int hit=mob_type(s,i)==3?16:mob_type(s,i)==2?10:6;s.health=std::max(0,int(s.health)-hit);s.invulnerable=950;
    }
    m.cooldown=1200;
   }
   continue;
  }
  m.cooldown=m.cooldown>elapsed?m.cooldown-elapsed:0;
  if(dist<23&&!m.cooldown&&mine_line(s,m.x,m.y,s.px,s.py)){m.windup=400;continue;}
  if(dist<=18||dist>128)continue;
  int speed=mob_type(s,i)==1?28:mob_type(s,i)==2?15:20;
  m.move_acc+=elapsed*speed;int amount=m.move_acc/1000;m.move_acc%=1000;
  for(int step=0;step<amount;++step) {
   int sx=(dx>0)-(dx<0),sy=(dy>0)-(dy<0);bool horizontal=std::abs(dx)>=std::abs(dy),moved=false;
   for(int attempt=0;attempt<2&&!moved;++attempt) {
    int mx=(horizontal!=bool(attempt))?sx:0,my=(horizontal!=bool(attempt))?0:sy;
    if((mx||my)&&mine_walkable(s,int(m.x)+mx,int(m.y)+my)) {m.x+=mx;m.y+=my;m.dir=mx?(mx>0?1:3):(my>0?2:0);moved=true;}
   }
   if(!moved)break;
  }
 }
 if(s.health)return false;
 s.gold-=std::min(500u,s.gold/10);s.health=40;s.energy=std::min(20,int(s.energy));s.minute=std::min(1500,int(s.minute)+120);
 s.region=unsigned(Region::Home);s.visited|=1u<<unsigned(Region::Home);s.px=16*16+8;s.py=17*16+8;s.mine_depth=s.mine_loaded=0;s.invulnerable=s.attack_cooldown=0;for(auto &m:s.mobs)m=MineMob{};std::fill(s.ore_damage,s.ore_damage+MineRocks,uint8_t(0));return true;
}
}
