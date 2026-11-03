// SPDX-License-Identifier: MIT
#include "../core.hpp"
#include <algorithm>
#include <array>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>
using namespace starsoil;
static State s, loaded;
static std::array<int, ItemCount> totals() {
  std::array<int, ItemCount> result{};
  for (auto a : s.bag) if (a.item) result[a.item] += a.count;
  for (auto a : s.chest) if (a.item) result[a.item] += a.count;
  for (auto a : s.drops) if (a.item) result[a.item] += a.count;
  return result;
}
static bool same(Slot a, Slot b) { return a.item == b.item && a.count == b.count; }
static void fresh() {
  new_game(s, 314159, WorldSize::Small);
  for (auto &a : s.bag) a = {};
  for (auto &a : s.chest) a = {};
  for (auto &a : s.drops) a = {};
  for (auto &a : s.mobs) a = {};
  s.invulnerable = 60000;
  set_tile(s, 50, 30, StorageChest);
}
static void inventory() {
  fresh(); s.selected = 23;
  s.bag[0] = {Wood, 5}; s.bag[1] = {WoodPick, 1};
  s.bag[8] = {Wood, 999}; s.bag[13] = {Wood, 16};
  s.bag[20] = {IronPick, 1}; s.bag[23] = {Wood, 7};
  s.bag[39] = {Wood, 21}; s.bag[18] = {HealPotion, 3};
  auto before = totals(); Slot hotbar[HotSlots]; std::copy_n(s.bag, HotSlots, hotbar);
  Slot hand = s.bag[s.selected];
  assert(sort_inventory(s));
  for (int i = 0; i < HotSlots; ++i) assert(same(s.bag[i], hotbar[i]));
  assert(s.selected == 23 && same(s.bag[23], hand) && totals() == before);
  assert(!sort_inventory(s));
  s.chest[0] = {Wood, 980};
  before = totals(); assert(quick_stack(s) == 1036);
  assert(same(s.bag[0],hotbar[0]) && same(s.bag[23],hand));
  assert(count(s, Wood) == 12 && totals() == before);
  assert(s.chest[0].count == 999 && s.chest[1].count == 999 && s.chest[2].count == 18);
  assert(quick_stack(s) == 0); assert(take_all(s) == 2016 && totals() == before);
  // Full target: transfer only what fits, leaving a valid source remainder.
  fresh(); for (auto &a : s.chest) a = {Rock,999};
  s.chest[5] = {Wood,997}; s.bag[20] = {Wood,50}; before=totals();
  assert(store(s,20) && s.chest[5].count==999 && s.bag[20].count==48 && totals()==before);
  assert(!store(s,20) && totals()==before);
  for (auto &a : s.bag) a = {Wood,999}; s.bag[20] = {Rock,995};
  before=totals(); assert(withdraw(s,0) && s.bag[20].count==999 && s.chest[0].count==995 && totals()==before);
  assert(!withdraw(s,0) && totals()==before);
  // A remote chest cannot be used by bulk actions.
  set_tile(s,50,30,Air); assert(!quick_stack(s) && !take_all(s));
  // Repeated mixed operations must conserve every item, not just stack count.
  fresh(); s.selected=29;
  for(int i=0;i<BagSlots;++i)s.bag[i]={uint8_t(1+(i*13)% (ItemCount-1)),uint16_t(1+(i*67)%999)};
  for(int i=0;i<12;++i)s.chest[i]={uint8_t(1+(i*7)%(ItemCount-1)),uint16_t(1+(i*37)%999)};
  before=totals();
  for(int i=0;i<1000;++i) {
    switch(i%7) {
      case 0:sort_inventory(s);break; case 1:sort_inventory(s,true);break;
      case 2:quick_stack(s);break;case 3:take_all(s);break;
      case 4:store(s,(i*13)%40);break;case 5:withdraw(s,(i*7)%12);break;
      default:move_stack(s,(i*19)%52,(i*31+3)%52);break;
    }
    assert(totals()==before);
    for(auto a:s.bag)assert(a.count<=999 && bool(a.item)==bool(a.count));
    for(auto a:s.chest)assert(a.count<=999 && bool(a.item)==bool(a.count));
  }
  std::vector<uint8_t> data(SaveBytes);auto n=encode(s,data.data(),data.size());
  assert(n && decode(loaded,data.data(),n) && loaded.selected==29);
  for(int i=0;i<BagSlots;++i)assert(same(s.bag[i],loaded.bag[i]));
  for(int i=0;i<12;++i)assert(same(s.chest[i],loaded.chest[i]));
  puts("PASS sorting, protected hotbar/held items, matching quick stack, partial transfers, take all, 1000 mixed item-conservation transactions and unchanged save format");
}
static void pickup() {
  fresh();for(auto &a:s.bag)a={Rock,999};s.bag[39]={Wood,997};
  s.drops[0]={s.player.x,s.player.y-10,0,Wood,50};auto before=totals();
  tick(s,{},20);assert(s.bag[39].count==999 && s.drops[0].count==48 && totals()==before);
  s.bag[39].count=950;before=totals();tick(s,{},20);
  assert(!s.drops[0].item && !s.drops[0].count && s.bag[39].count==998 && totals()==before);
  puts("PASS partial ground pickup preserves remainder and clears completed drops");
}
static void crafting() {
  fresh();assert(craft_status(s,-1)==CraftStatus::Invalid);
  assert(craft_status(s,0)==CraftStatus::Ingredients);add(s,Wood,1);
  assert(craft_status(s,0)==CraftStatus::Ready);
  set_tile(s,44,30,Air);assert(craft_status(s,4)==CraftStatus::Station);
  for(auto&a:s.bag)a={WoodPick,999};s.bag[0]={Wood,2};
  assert(craft_status(s,0)==CraftStatus::Space);
  auto original=s.bag[0];assert(!craft(s,0) && same(original,s.bag[0]));
  s.bag[0].count=1;assert(craft(s,0) && count(s,Boards)==4);
  puts("PASS distinct crafting failure reasons, no material loss on failure and space freed by consumed ingredients");
}
static void arena() {
  fresh();std::fill_n(s.tiles,Cells,uint8_t(Air));std::fill_n(s.walls,Cells,uint8_t(0));
  for(int x=1;x<s.width-1;++x)set_tile(s,x,32,Stone);
  s.player={100*16+8,32*16,0,0,100,0,1};tick(s,{},20);s.spawn_timer=0;
}
static void jumping() {
  Input jump; jump.jump=true;
  arena();set_tile(s,100,32,Air);set_tile(s,99,32,Air);
  tick(s,{},20);assert(!s.player.grounded);
  tick(s,{},20);tick(s,jump,20);assert(s.player.vy < -280 && s.jumps==1);
  // A new press after grace expires cannot produce an ordinary air jump.
  arena();for(int x=95;x<106;++x)set_tile(s,x,32,Air);
  for(int i=0;i<7;++i)tick(s,{},20);
  tick(s,jump,20);assert(s.player.vy>0 && s.jumps==0);
  // Press just before landing: buffered jump is consumed exactly once.
  arena();s.player.grounded=false;s.player.y-=8;s.player.vy=100;s.ground_grace=0;
  tick(s,jump,20);assert(s.player.vy>0 && s.jump_buffer>0);
  for(int i=0;i<8 && s.jumps==0;++i)tick(s,jump,20);
  assert(s.jumps==1 && s.player.vy<0 && s.jump_buffer==0);
  for(int i=0;i<90;++i)tick(s,jump,20);
  assert(s.player.grounded && s.player.vy==0 && s.jumps==0);
  // Returning home clears air-jump state instead of carrying it across teleport.
  s.jump_buffer=80;s.ground_grace=80;s.jump_down=true;s.jumps=2;
  respawn(s);assert(!s.jump_buffer&&!s.ground_grace&&!s.jump_down&&!s.jumps);
  puts("PASS 100ms edge grace, 120ms landing buffer, no unlimited air jumps or held auto-bounce, teleport input reset");
}
static void home_and_garden() {
  arena();s.home_x=100;s.home_y=32;set_tile(s,100,31,Stone);
  auto edited=std::vector<uint8_t>(s.tiles,s.tiles+Cells);
  respawn(s);assert(s.player.y>=32 && (s.player.x!=100*16 || s.player.y!=32*16));
  assert(std::equal(edited.begin(),edited.end(),s.tiles));
  arena();s.home_x=100;s.home_y=32;set_tile(s,100,31,Lava);
  respawn(s);assert(s.player.x!=100*16 || s.player.y!=32*16);
  // Saplings on new raised ground, far from the original height map, grow.
  arena();set_tile(s,110,20,Sapling);set_tile(s,110,21,Dirt);
  set_tile(s,116,20,Sapling);set_tile(s,116,21,Dirt);set_tile(s,116,14,Plank);
  set_tile(s,122,20,Sapling); // Unsupported sapling must remain a sapling.
  for(int i=0;i<1650;++i){s.spawn_timer=0;tick(s,{},40);}
  assert(tile(s,110,20)==Log && tile(s,110,16)==Log && tile(s,108,13)==Leaves);
  assert(tile(s,116,20)==Sapling && tile(s,116,14)==Plank && tile(s,122,20)==Sapling);
  // Setting home at the bottom edge used to encode home_y outside the allowed
  // interior, making the next load reject an otherwise valid world.
  for(auto size:{WorldSize::Small,WorldSize::Medium,WorldSize::Large}) {
    new_game(s,314159,size);int y=s.height-2;
    for(int yy=y-3;yy<=y;++yy)for(int x=96;x<=104;++x)set_tile(s,x,yy,Air);
    set_tile(s,100,y,Bed);s.player.x=100*16;s.player.y=y*16;
    assert(interact_at(s,100,y) && s.home_y<s.height-1);
    std::vector<uint8_t> wire(SaveBytes);auto n=encode(s,wire.data(),wire.size());
    assert(n && decode(loaded,wire.data(),n) && loaded.home_y==s.home_y);
  }
  puts("PASS blocked/lava home recovery without terrain edits; trees grow on edited terrain while respecting roofs and soil support");
}
int main(){inventory();pickup();crafting();jumping();home_and_garden();}
