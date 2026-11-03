// SPDX-License-Identifier: MIT
#include "../core.hpp"
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>
using namespace starsoil;
static State s, loaded;
static std::vector<int> totals() {
  std::vector<int> out(ItemCount);
  for (auto slot : s.bag) out[slot.item] += slot.count;
  for (auto slot : s.chest) out[slot.item] += slot.count;
  return out;
}
int main() {
  new_game(s, 314159);
  for (auto &slot : s.bag) slot = {};
  for (auto &slot : s.chest) slot = {};
  s.bag[0] = {Wood,990}; s.bag[1] = {Wood,20}; s.bag[39] = {IronAxe,1};
  assert(move_stack(s,1,0) && s.bag[0].count==999 && s.bag[1].count==11);
  auto original = s;
  assert(!move_stack(s,1,0) && !move_stack(s,1,1) && !move_stack(s,2,4));
  for (auto [from,to] : {std::pair{-1,0}, {0,-1}, {52,0}, {0,52}}) assert(!move_stack(s,from,to));
  assert(std::memcmp(s.bag,original.bag,sizeof(s.bag))==0);
  assert(move_stack(s,39,51) && !s.bag[39].item && s.chest[11].item==IronAxe);
  assert(move_stack(s,51,0) && s.bag[0].item==IronAxe && s.chest[11].count==999);
  auto expected=totals();
  for (int i=0;i<500;++i) {
    move_stack(s,(i*37+7)%52,(i*19+23)%52);
    assert(totals()==expected);
    for (auto slot:s.bag) assert(slot.count<=999 && bool(slot.item)==bool(slot.count));
    for (auto slot:s.chest) assert(slot.count<=999 && bool(slot.item)==bool(slot.count));
  }
  puts("PASS bounded inventory transactions: swaps, partial/full merges, cross-container moves and item conservation");
  const auto hotbar = std::vector<Slot>(s.bag,s.bag+HotSlots);
  for (int id : {WoodPick,IronAxe,IronSword,HunterBow,StarWand,Boards,Doors}) {
    s.bag[39]={uint8_t(id),1};
    assert(activate(s,39) && s.selected==39 && s.bag[39].item==id && s.bag[39].count==1);
    assert(std::memcmp(s.bag,hotbar.data(),sizeof(Slot)*HotSlots)==0);
  }
  s.bag[20]={HealPotion,2}; s.hp=50;
  assert(activate(s,20) && s.hp==95 && s.bag[20].count==1);
  s.bag[21]={CopperHelmet,1}; assert(activate(s,21) && s.equip[0]==CopperHelmet);
  s.bag[39]={IronPick,1}; assert(activate(s,39));
  for (int y=20;y<33;++y) for (int x=35;x<85;++x) set_tile(s,x,y,y==32?Stone:Air);
  s.player={48*16,32*16,0,0,100,0,1}; s.invulnerable=60000;
  for (auto &m:s.mobs)m={};
  s.mobs[0]={s.player.x+25,s.player.y,0,0,4,1,1};
  set_tile(s,50,31,Stone);
  Input use;use.use=true;use.target_x=50;use.target_y=31;
  auto mined=s.mined,kills=s.kills;
  for(int i=0;i<30;++i)tick(s,use,20);
  assert(s.mined==mined+1 && s.kills==kills+1 && tile(s,50,31)==Air && s.selected==39);
  s.bag[20]={Boards,5}; assert(activate(s,20)); s.cooldown=0;
  use.target_y=30; tick(s,use,20);
  assert(tile(s,50,30)==Plank && s.bag[20].count==4 && s.selected==20);
  std::vector<uint8_t> wire(SaveBytes);
  size_t n=encode(s,wire.data(),wire.size());assert(n && decode(loaded,wire.data(),n));
  assert(loaded.selected==20 && loaded.bag[20].count==4);
  s.selected=39;n=encode(s,wire.data(),wire.size());assert(n && decode(loaded,wire.data(),n) && loaded.selected==39);
  s.selected=40;n=encode(s,wire.data(),wire.size());assert(n && !decode(loaded,wire.data(),n) && loaded.selected==39);
  puts("PASS direct use from all backpack rows: wielding, mining/combat, building, potions, equipment and saved selection beyond the hotbar");
}
