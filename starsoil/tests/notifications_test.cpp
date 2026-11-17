// SPDX-License-Identifier: MIT
#include "../core.hpp"
#include "../effects.hpp"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>
using namespace starsoil;
static State s,restored;
int main() {
  starsoil_notices::Gate gate;
  assert(gate.accept("mana",NoticeKind::Warning,100,false));
  assert(!gate.accept("mana",NoticeKind::Warning,200,true));
  assert(!gate.accept("mana",NoticeKind::Warning,2200,false));
  assert(gate.accept("mana",NoticeKind::Warning,3000,false));
  assert(gate.accept("boss",NoticeKind::Milestone,3100,true));
  assert(!gate.accept("mana",NoticeKind::Warning,3200,true));
  assert(gate.accept("mana",NoticeKind::Warning,8000,false));
  assert(starsoil_notices::active(0xfffffff0u,24));
  assert(!starsoil_notices::active(25,24));assert(!starsoil_notices::active(0xfffffff0u,0));
  puts("PASS notice priority, repeat cooldown and timer wrap; no unbounded queue");
  new_game(s,1234567);assert(!s.event_serial && !s.event[0]);
  s.player.x=51*16+8;s.player.y=31*16;
  for(int i=0;i<8;++i) {assert(interact_at(s,53,30));assert(!s.event_serial);}
  assert(equip(s,0)==false); // Held pick is not wearable.
  add(s,CopperHelmet,1);int slot=-1;for(int i=0;i<BagSlots;++i)if(s.bag[i].item==CopperHelmet)slot=i;
  assert(slot>=0 && equip(s,slot) && !s.event_serial);
  assert(craft(s,0) && !s.event_serial);
  std::vector<uint8_t> save(SaveBytes);size_t size=encode(s,save.data(),save.size());
  assert(size && decode(restored,save.data(),size) && !restored.event_serial && !restored.event[0]);
  // A blocked close is actionable and must still report a warning.
  assert(interact_at(s,53,30));s.player.x=53*16+8;
  assert(!interact_at(s,53,30) && s.event_kind==NoticeKind::Warning && s.event_serial);
  s.player.x=51*16+8;assert(interact_at(s,49,30) && s.event_kind==NoticeKind::Info);
  puts("PASS doors/equipment/crafting/load are quiet; blocked doors and home changes remain visible; original save format round trips");
  Effects effects;
  for(int i=0;i<100;++i)effects.pop(100+i,200,1,0xffffff,Wood);
  unsigned pickups=0;for(auto p:effects.pops)if(p.life && p.item){++pickups;assert(p.amount==100 && p.x==199 && p.y==200);}
  assert(pickups==1);
  for(int id=Soil;id<Soil+12;++id)effects.pop(100,200,1,0xffffff,uint8_t(id));
  pickups=0;for(auto p:effects.pops)pickups+=p.life && p.item;assert(pickups<=3);
  puts("PASS repeated pickups merge at the latest location and no more than three distinct pickup lines remain active");
}
