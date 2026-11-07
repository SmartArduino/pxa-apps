// SPDX-License-Identifier: MIT
#include "../core.hpp"
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstring>
using namespace farm;
static uint32_t sum(const uint8_t *data) { uint32_t h=2166136261u;for(int i=0;i<SaveBytes-4;++i)h=(h^data[i])*16777619u;return h; }
static void resign(uint8_t *data) {auto h=sum(data);for(int i=0;i<4;++i)data[SaveBytes-4+i]=h>>(i*8);}
static void valid_inventory(const State&s) {
  bool found[ItemCount+1]{};
  for(auto id:s.slots)if(id){assert(id<=ItemCount&&!found[id]&&item_count(s,id));found[id]=true;}
  for(int id=1;id<=ItemCount;++id)assert(found[id]==bool(item_count(s,id)));
}
int main() {
  for(Tool tool:{Tool::Axe,Tool::Pick})for(int grade=0;grade<3;++grade){
    State s;fresh(s);int index=tool==Tool::Axe?0:1;
    const auto&o=Obstacles[index];s.px=o.x*16+8;s.py=(o.y+1)*16+8;
    s.tool=uint8_t(tool);s.tool_grade[unsigned(tool)]=grade;
    int required=(obstacle_hp(index)+grade)/(1+grade);
    assert(strikes_left(s,index,tool)==required);
    for(int hit=1;hit<required;++hit){assert(act(s,o.x,o.y)==Result::Struck);assert(!s.cleared[index]);assert(strikes_left(s,index,tool)==required-hit);}
    assert(act(s,o.x,o.y)==Result::Cleared && s.cleared[index] && !s.damage[index]);
    auto loot=tool==Tool::Axe?s.wood:s.stone;act(s,o.x,o.y);assert(loot==(tool==Tool::Axe?s.wood:s.stone));
    if(tool==Tool::Axe)assert(required==(grade==0?5:grade==1?3:2));
  }
  State s;fresh(s);s.gold=500;s.wood=100;s.stone=100;
  s.px=168;s.py=168;s.tool=uint8_t(Tool::Axe);
  assert(act(s,10,9)==Result::Struck && s.damage[0]==1);
  assert(upgrade_tool(s,Tool::Axe)&&s.tool_grade[4]==1&&s.damage[0]==1);
  assert(upgrade_tool(s,Tool::Axe)&&s.tool_grade[4]==2);
  assert(!upgrade_tool(s,Tool::Axe)&&!upgrade_tool(s,Tool::Hoe));
  s.fish=5;for(int i=0;i<4;++i){s.seeds[i]=i+10;s.produce[i]=i+3;}sync_inventory(s);valid_inventory(s);
  assert(swap_slots(s,0,27)&&!s.slots[0]&&s.slots[27]==1);
  assert(select_slot(s,-1)&&s.active_slot==255&&s.tool==uint8_t(Tool::Hand));
  assert(equip_item(s,17)&&s.slots[s.active_slot]==17&&s.tool==uint8_t(Tool::Hand));
  auto before=s;
  for(int i=0;i<200;++i){assert(swap_slots(s,i%28,(i*13+7)%28)==(i%28!=(i*13+7)%28));valid_inventory(s);}
  assert(s.gold==before.gold&&s.wood==before.wood&&s.stone==before.stone&&s.fish==before.fish);
  assert(!std::memcmp(s.seeds,before.seeds,sizeof(s.seeds))&&!std::memcmp(s.produce,before.produce,sizeof(s.produce)));
  uint8_t bytes[SaveBytes];select_slot(s,-1);s.show_joystick=false;s.show_action=false;s.music=true;
  assert(encode(s,bytes,sizeof(bytes))==SaveBytes && bytes[4]==6);
  State restored;assert(decode(restored,bytes,SaveBytes));valid_inventory(restored);
  assert(restored.damage[0]==1&&restored.tool_grade[4]==2&&restored.active_slot==255&&!restored.show_action&&!restored.show_joystick&&restored.music);
  assert(!std::memcmp(s.slots,restored.slots,sizeof(s.slots)));
  bytes[450]=bytes[449];resign(bytes);assert(!decode(restored,bytes,SaveBytes));
  fresh(s);s.gold=321;s.wood=12;s.tool=6;encode(s,bytes,sizeof(bytes));bytes[4]=1;std::fill(bytes+418,bytes+508,0);uint32_t legacy=2166136261u;for(int i=0;i<508;++i)legacy=(legacy^bytes[i])*16777619u;for(int i=0;i<4;++i)bytes[508+i]=legacy>>(i*8);
  assert(decode(restored,bytes,512)&&restored.gold==321&&restored.wood==12&&restored.active_slot==6&&restored.tree_age[0]==5&&restored.show_joystick);valid_inventory(restored);
  fresh(s);assert(tree_stage(s,16)==0&&walkable(s,Obstacles[16].x,Obstacles[16].y));
  for(int day=0;day<5;++day)sleep(s);assert(tree_stage(s,16)==2);
  auto o=Obstacles[16];s.px=o.x*16+8;s.py=(o.y+1)*16+8;s.tool=4;
  for(int i=0;i<obstacle_hp(16);++i)act(s,o.x,o.y);
  assert(s.cleared[16]&&tree_stage(s,16)==3);sleep(s);assert(s.cleared[16]);sleep(s);assert(!s.cleared[16]&&tree_stage(s,16)==0);
  s.cleared[0]=1;s.tree_age[0]=0;s.plots[0].tilled=1;for(int day=0;day<8;++day)sleep(s);assert(s.cleared[0]);
  puts("PASS: 5/3/2 material strikes, persistent damage, upgrade costs, lossless inventory swaps, empty hands, v1 migration, settings, tree growth and protected farmland");
}
