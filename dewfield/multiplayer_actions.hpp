// SPDX-License-Identifier: MIT
#pragma once
#include "multiplayer.hpp"
namespace dewfield_actions {
using dewfield_net::room;using dewfield_net::Op;
inline farm::Result act(farm::State&s,int a,int b) {
  if(!room.active()&&!room.guest())return farm::act(s,a,b);
  return farm::Result(room.local(Op::Act,a,b));
}
inline farm::Result mine_strike(farm::State&s,int a,int b) {
  if(!room.active()&&!room.guest())return farm::mine_strike(s,a,b);
  return farm::Result(room.local(Op::Strike,a,b));
}
inline bool buy_seed(farm::State&s,int a,int b=1) {
  if(!room.active()&&!room.guest())return farm::buy_seed(s,a,b);
  return bool(room.local(Op::BuySeed,a,b));
}
inline bool upgrade(farm::State&s) {
  if(!room.active()&&!room.guest())return farm::upgrade(s);
  return bool(room.local(Op::UpgradeWater));
}
inline bool upgrade_tool(farm::State&s,farm::Tool a) {
  if(!room.active()&&!room.guest())return farm::upgrade_tool(s,a);
  return bool(room.local(Op::UpgradeTool,int(a)));
}
inline bool craft_sprinkler(farm::State&s,int a) {
  if(!room.active()&&!room.guest())return farm::craft_sprinkler(s,a);
  return bool(room.local(Op::Sprinkler,a));
}
inline uint32_t ship_items(farm::State&s,int a,unsigned b) {
  if(!room.active()&&!room.guest())return farm::ship_items(s,a,b);
  return uint32_t(room.local(Op::Ship,a,int(b)));
}
inline bool unship(farm::State&s) {
  if(!room.active()&&!room.guest())return farm::unship(s);
  return bool(room.local(Op::Unship));
}
inline bool eat(farm::State&s,int a) {
  if(!room.active()&&!room.guest())return farm::eat(s,a);
  return bool(room.local(Op::Eat,a));
}
inline bool chat(farm::State&s,int a) {
  if(!room.active()&&!room.guest())return farm::chat(s,a);
  return bool(room.local(Op::Chat,a));
}
inline bool gift(farm::State&s,int a,int b) {
  if(!room.active()&&!room.guest())return farm::gift(s,a,b);
  return bool(room.local(Op::Gift,a,b));
}
inline bool personal_event(farm::State&s,int a) {
  if(!room.active()&&!room.guest())return farm::personal_event(s,a);
  return bool(room.local(Op::PersonalEvent,a));
}
inline bool advance_story(farm::State&s,int a) {
  if(!room.active()&&!room.guest())return farm::advance_story(s,a);
  return bool(room.local(Op::Story,a));
}
inline bool friend_story(farm::State&s,int a) {
  if(!room.active()&&!room.guest())return farm::friend_story(s,a);
  return bool(room.local(Op::FriendStory,a));
}
inline bool ranch_story(farm::State&s,int a) {
  if(!room.active()&&!room.guest())return farm::ranch_story(s,a);
  return bool(room.local(Op::RanchStory,a));
}
inline bool deliver_order(farm::State&s,int a) {
  if(!room.active()&&!room.guest())return farm::deliver_order(s,a);
  return bool(room.local(Op::Order,a));
}
inline bool cook(farm::State&s,int a) {
  if(!room.active()&&!room.guest())return farm::cook(s,a);
  return bool(room.local(Op::Cook,a));
}
inline bool claim_collection_reward(farm::State&s,int a) {
  if(!room.active()&&!room.guest())return farm::claim_collection_reward(s,a);
  return bool(room.local(Op::Collection,a));
}
inline bool buy_animal(farm::State&s,int a,bool b) {
  if(!room.active()&&!room.guest())return farm::buy_animal(s,a,b);
  return bool(room.local(Op::BuyAnimal,a,int(b)));
}
inline bool buy_hay(farm::State&s) {
  if(!room.active()&&!room.guest())return farm::buy_hay(s);
  return bool(room.local(Op::BuyHay));
}
inline bool fill_trough(farm::State&s) {
  if(!room.active()&&!room.guest())return farm::fill_trough(s);
  return bool(room.local(Op::Feed));
}
inline bool pet_animal(farm::State&s,int a) {
  if(!room.active()&&!room.guest())return farm::pet_animal(s,a);
  return bool(room.local(Op::Pet,a));
}
inline int collect_animal(farm::State&s,int a) {
  if(!room.active()&&!room.guest())return farm::collect_animal(s,a);
  return int(room.local(Op::CollectAnimal,a));
}
inline bool incubate(farm::State&s,int a) {
  if(!room.active()&&!room.guest())return farm::incubate(s,a);
  return bool(room.local(Op::Incubate,a));
}
inline bool slaughter(farm::State&s,int a) {
  if(!room.active()&&!room.guest())return farm::slaughter(s,a);
  return bool(room.local(Op::Slaughter,a));
}
inline bool enter_mine(farm::State&s,int a=1) {
  if(!room.active()&&!room.guest())return farm::enter_mine(s,a);
  return bool(room.local(Op::EnterMine,a));
}
inline bool descend_mine(farm::State&s) {
  if(!room.active()&&!room.guest())return farm::descend_mine(s);
  return bool(room.local(Op::Descend));
}
inline bool mine_treasure(farm::State&s) {
  if(!room.active()&&!room.guest())return farm::mine_treasure(s);
  return bool(room.local(Op::Treasure));
}
inline bool forge_sword(farm::State&s) {
  if(!room.active()&&!room.guest())return farm::forge_sword(s);
  return bool(room.local(Op::ForgeSword));
}
inline bool spend_energy(farm::State&s,int a) {
  if(!room.active()&&!room.guest())return farm::spend_energy(s,a);
  return bool(room.local(Op::SpendEnergy,a));
}
inline void begin_ranch(farm::State&s) {if(!room.active()&&!room.guest()){farm::begin_ranch(s);return;}(void)room.local(Op::Ranch);}
inline void leave_mine(farm::State&s) {if(!room.active()&&!room.guest()){farm::leave_mine(s);return;}(void)room.local(Op::LeaveMine);}
}
