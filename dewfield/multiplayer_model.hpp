// SPDX-License-Identifier: MIT
#pragma once
#include "core.hpp"
#include "world.hpp"
#include "life.hpp"
#include "livestock.hpp"
#include "mines.hpp"
#include "collections.hpp"
#include "fishing.hpp"
#include <algorithm>
#include <cstring>

namespace dewfield_net {
constexpr unsigned MaxPlayers=4, MaxPacket=1536, DefaultPort=31889;
constexpr unsigned PoseBytes=24, CommandBytes=12;
constexpr unsigned ProtocolVersion=4;
enum class Mode : uint8_t { Offline, Hosting, Joining, Guest };
enum class Message : uint8_t { Hello=1, Welcome, Pose, Command, Snapshot, Goodbye };
enum class Op : uint8_t {
  Act, Strike, BuySeed, UpgradeWater, UpgradeTool, Sprinkler, Ship, Unship,
  Eat, Chat, Gift, PersonalEvent, Story, FriendStory, RanchStory, Order, Cook,
  Collection, Ranch, BuyAnimal, BuyHay, Feed, Pet, CollectAnimal, Incubate,
  Slaughter, EnterMine, LeaveMine, Descend, Treasure, ForgeSword,
  FishBegin, FishPress, SpendEnergy, Count
};
struct Command {uint32_t sequence=0;Op op=Op::Act;int16_t a=0,b=0;uint16_t elapsed=0;};
inline uint16_t get16(const uint8_t*p){return uint16_t(p[0]|unsigned(p[1])<<8);}
inline uint32_t get32(const uint8_t*p){return get16(p)|uint32_t(get16(p+2))<<16;}
inline void put16(uint8_t*p,unsigned n){p[0]=uint8_t(n);p[1]=uint8_t(n>>8);}
inline void put32(uint8_t*p,uint32_t n){put16(p,n);put16(p+2,n>>16);}
struct Pose {
  uint16_t x=232,y=232,swing=0,distance=0;
  uint8_t region=0,depth=0,dir=2,tool=3,item=0,seed=0,grade=0,frame=0;
  int16_t target_x=0,target_y=0;
  bool active=false,walking=false;
  uint8_t appearance=0;
};
inline void encode_pose(const Pose&p,uint8_t*out){
  std::memset(out,0,PoseBytes);put16(out,p.x);put16(out+2,p.y);
  put16(out+4,p.swing);put16(out+6,p.distance);out[8]=p.region;out[9]=p.depth;
  out[10]=p.dir;out[11]=p.tool;out[12]=p.item;out[13]=p.seed;out[14]=p.grade;out[15]=p.frame;
  put16(out+16,uint16_t(p.target_x));put16(out+18,uint16_t(p.target_y));
  out[20]=uint8_t(p.active)|(uint8_t(p.walking)<<1);out[21]=p.appearance;
}
inline bool decode_pose(Pose&p,const uint8_t*in,size_t n){
  if(n!=PoseBytes)return false;
  Pose v;v.x=get16(in);v.y=get16(in+2);v.swing=get16(in+4);v.distance=get16(in+6);
  v.region=in[8];v.depth=in[9];v.dir=in[10];v.tool=in[11];v.item=in[12];v.seed=in[13];
  v.grade=in[14];v.frame=in[15];v.target_x=int16_t(get16(in+16));v.target_y=int16_t(get16(in+18));
  v.appearance=in[21];v.active=in[20]&1;v.walking=in[20]&2;
  if(v.x>=farm::MapW*16||v.y>=farm::MapH*16||v.swing>2000||v.region>=farm::RegionCount||
     v.depth>farm::MineFloors||(v.region==unsigned(farm::Region::Mine))!=bool(v.depth)||
     v.dir>3||v.tool>=unsigned(farm::Tool::Count)||v.item>farm::ItemCount||v.seed>3||
     v.grade>3||v.frame>8||in[20]>3||in[21]>1||in[22]||in[23]||
     v.target_x<0||v.target_x>=512||v.target_y<0||v.target_y>=384)return false;
  p=v;return true;
}
inline void encode_command(const Command&c,uint8_t*out){
  put32(out,c.sequence);out[4]=uint8_t(c.op);out[5]=0;put16(out+6,uint16_t(c.a));
  put16(out+8,uint16_t(c.b));put16(out+10,c.elapsed);
}
inline bool decode_command(Command&c,const uint8_t*in,size_t n){
  if(n!=CommandBytes||!get32(in)||in[4]>=unsigned(Op::Count)||in[5])return false;
  c={get32(in),Op(in[4]),int16_t(get16(in+6)),int16_t(get16(in+8)),get16(in+10)};return true;
}
// Only these fields belong to an individual farmer. Quantities, gold, world
// progress and upgrades remain shared. Local hotbar layout stays personal.
struct Personal {
  uint16_t x=232,y=232,energy=100,water=30,invulnerable=0,cooldown=0;
  uint8_t appearance=0;
  uint8_t region=0,depth=0,health=100,overwork=0,tool=3,seed=0,slot=255;
  uint8_t slots[farm::InventorySize]{};
  bool exhausted=false,joystick=true,action=true,music=true,run=true,effects=true;
  void capture(const farm::State&s){
    appearance=s.appearance;x=s.px;y=s.py;region=s.region;depth=s.mine_depth;energy=s.energy;water=s.water;health=s.health;
    invulnerable=s.invulnerable;cooldown=s.attack_cooldown;overwork=s.overwork;exhausted=s.exhausted;
    tool=s.tool;seed=s.selected;slot=s.active_slot;std::memcpy(slots,s.slots,sizeof slots);
    joystick=s.show_joystick;action=s.show_action;music=s.music;run=s.auto_run;effects=s.sound_effects;
  }
  void install(farm::State&s)const{
    s.appearance=appearance;s.px=x;s.py=y;s.region=region;s.mine_depth=depth;s.mine_loaded=depth;s.visited|=1u<<region;
    s.energy=energy;s.water=std::min(water,s.capacity);s.health=health;
    s.invulnerable=invulnerable;s.attack_cooldown=cooldown;s.overwork=overwork;s.exhausted=exhausted;
    s.tool=tool;s.selected=seed;s.active_slot=slot;std::memcpy(s.slots,slots,sizeof slots);
    s.show_joystick=joystick;s.show_action=action;s.music=music;s.auto_run=run;s.sound_effects=effects;
    farm::sync_inventory(s);
  }
};
struct Farmer {
  Personal personal;
  Pose pose,previous;
  farm::FishingSession fishing;
  uint32_t last_command=0,last_seen=0,received_at=0,snapshot_hash=0,snapshot_at=0,command_window=0;
  unsigned command_count=0;
  uint32_t fishing_started=0;
  int last_result=0;
};
inline bool valid_command(const Command&c){
  switch(c.op){
    case Op::Act:case Op::Strike:return c.a>=0&&c.a<32&&c.b>=0&&c.b<24;
    case Op::BuySeed:return c.a>=0&&c.a<5&&c.b>=1&&c.b<=99;
    case Op::UpgradeTool:return c.a>=0&&c.a<7;
    case Op::Sprinkler:return c.a>=0&&c.a<farm::PlotCount;
    case Op::Ship:return c.a>=1&&c.a<=farm::ItemCount&&c.b>=1&&c.b<=999;
    case Op::Eat:case Op::Gift:return c.op==Op::Eat?c.a>=1&&c.a<=farm::ItemCount:
      c.a>=0&&c.a<farm::NpcCount&&c.b>=1&&c.b<=farm::ItemCount;
    case Op::Chat:case Op::PersonalEvent:case Op::Story:case Op::FriendStory:
    case Op::RanchStory:case Op::Order:return c.a>=0&&c.a<farm::NpcCount;
    case Op::Cook:return c.a>=0&&c.a<int(sizeof(farm::Recipes)/sizeof(*farm::Recipes));
    case Op::Collection:return c.a>=0&&c.a<farm::MilestoneCount;
    case Op::BuyAnimal:return c.a>=1&&c.a<=5&&(c.b==0||c.b==1);
    case Op::Pet:case Op::CollectAnimal:case Op::Slaughter:return c.a>=0&&c.a<farm::AnimalMax;
    case Op::Incubate:return c.a>=1&&c.a<=2;
    case Op::EnterMine:return c.a>=1&&c.a<=farm::MineFloors;
    case Op::SpendEnergy:return c.a>=1&&c.a<=10;
    case Op::FishPress:return c.elapsed<=12000;
    default:return c.op<Op::Count;
  }
}
// Server replays typed operations against current quantities. It never
// accepts a replacement State or an arbitrary inventory/gold delta.
inline int execute(farm::State&s,Farmer&f,const Command&c,uint32_t now){
  using namespace farm;
  if(!valid_command(c))return -1;
  switch(c.op){
    case Op::Act:return int(act(s,c.a,c.b));
    case Op::Strike:return int(mine_strike(s,c.a,c.b));
    case Op::BuySeed:return buy_seed(s,c.a,c.b);
    case Op::UpgradeWater:return upgrade(s);
    case Op::UpgradeTool:return upgrade_tool(s,Tool(c.a));
    case Op::Sprinkler:return craft_sprinkler(s,c.a);
    case Op::Ship:return int(ship_items(s,c.a,unsigned(c.b)));
    case Op::Unship:return unship(s);
    case Op::Eat:return eat(s,c.a);
    case Op::Chat:return chat(s,c.a);
    case Op::Gift:return gift(s,c.a,c.b);
    case Op::PersonalEvent:return personal_event(s,c.a);
    case Op::Story:return advance_story(s,c.a);
    case Op::FriendStory:return friend_story(s,c.a);
    case Op::RanchStory:return ranch_story(s,c.a);
    case Op::Order:return deliver_order(s,c.a);
    case Op::Cook:return cook(s,c.a);
    case Op::Collection:return claim_collection_reward(s,c.a);
    case Op::Ranch:begin_ranch(s);return 1;
    case Op::BuyAnimal:return buy_animal(s,c.a,c.b!=0);
    case Op::BuyHay:return buy_hay(s);
    case Op::Feed:return fill_trough(s);
    case Op::Pet:return pet_animal(s,c.a);
    case Op::CollectAnimal:return collect_animal(s,c.a);
    case Op::Incubate:return incubate(s,c.a);
    case Op::Slaughter:return slaughter(s,c.a);
    case Op::EnterMine:return enter_mine(s,c.a);
    case Op::LeaveMine:leave_mine(s);return 1;
    case Op::Descend:return descend_mine(s);
    case Op::Treasure:return mine_treasure(s);
    case Op::ForgeSword:return forge_sword(s);
    case Op::SpendEnergy:return spend_energy(s,c.a);
    case Op::FishBegin:
      if(indoors(s.region)||!s.bait||s.energy<4||s.fish>=999)return 0;
      f.fishing.begin(s);f.fishing_started=now;return 1;
    case Op::FishPress:
      if(c.elapsed>now-f.fishing_started+300)return 0;
      f.fishing.elapsed=c.elapsed;
      if(f.fishing.phase==FishingSession::Phase::Waiting)f.fishing.tick(0);
      return f.fishing.press(s);
    default:return -1;
  }
}
} // namespace dewfield_net
