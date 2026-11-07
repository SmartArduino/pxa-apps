// SPDX-License-Identifier: MIT
#include "../life.hpp"
#include <cassert>
#include <cstring>
#include <cstdio>
using namespace farm;
static void stand(State&s,Region r,int x,int y) {
  s.region=unsigned(r);s.visited|=1u<<s.region;s.px=x*16+8;s.py=y*16+8;
}
int main() {
  State s;fresh(s);uint8_t bytes[SaveBytes];State restored;
  // All original entrances and the orchard connect by walkable arrivals.
  s.story=3;s.minute=600;
  for(const auto&p:Portals) {
    stand(s,p.from,p.x,p.y);assert(travel(s,p));
    assert(walkable(s,s.px/16,s.py/16));
  }
  for(int r=0;r<RegionCount;++r) {
    s.region=r;
    for(int to=0;to<RegionCount;++to)if(to!=r)assert(next_portal(s,Region(to))>=0);
  }
  fresh(s);s.day=29;stand(s,Region::Orchard,7,9);
  assert(collect_forage(s,6)&&s.gathered[0]==2&&!collect_forage(s,6));
  assert(forage_at(s,7,8)==-1);
  // Winter removes apples/berries but leaves mushrooms, with daily refresh.
  s.day=85;s.forage=0;assert(!forage_available(s,6)&&!forage_available(s,10));
  assert(forage_available(s,8));s.day=29;s.forage=0;
  s.gathered[0]=999;assert(!collect_forage(s,6)&&!(s.forage&(1u<<6)));
  s.gathered[0]=998;assert(collect_forage(s,6)&&s.gathered[0]==999);
  // A full output stack and missing ingredients never consume anything.
  stand(s,Region::Home,21,11);s.gathered[1]=4;s.produce[1]=2;s.meals[0]=999;
  assert(!cook(s,0)&&s.gathered[1]==4&&s.produce[1]==2);
  s.meals[0]=0;assert(cook(s,0)&&s.gathered[1]==2&&s.produce[1]==1&&s.meals[0]==1);
  s.energy=90;assert(eat(s,22)&&s.energy==100&&s.meals[0]==0);
  assert(!eat(s,20)&&s.gathered[1]==2);s.energy=10;
  assert(eat(s,20)&&s.energy==16&&s.gathered[1]==1);
  assert(!cook(s,0)&&s.produce[1]==1&&s.gathered[1]==1);
  assert(!cook(s,-1)&&!cook(s,2));
  // Cooking cannot happen remotely, even if every ingredient is present.
  s.gathered[0]=3;s.gathered[2]=1;stand(s,Region::Farm,14,14);
  assert(!cook(s,1)&&s.gathered[0]==3);
  stand(s,Region::Home,21,11);assert(cook(s,1)&&s.meals[1]==1);
  assert(gift(s,0,23)&&s.meals[1]==0&&s.bonds[0]==6);
  // Every request requires its exact goods and a visit to the actual board.
  fresh(s);auto rng=s.rng;
  for(int npc=0;npc<4;++npc) {
    auto order=daily_order(s,npc);assert(order.item>=12&&order.item<=20&&order.count);
    assert(s.rng==rng);assert(change_item(s,order.item,order.count));
    assert(!deliver_order(s,npc));
    stand(s,Region::Town,18,11);
    auto gold=s.gold;assert(deliver_order(s,npc));
    assert(s.gold==gold+order.reward&&s.bonds[npc]==3);
    assert(!deliver_order(s,npc)&&daily_order(s,npc).done);
    stand(s,Region::Farm,14,14);
  }
  assert(s.orders_done==4&&s.order_claimed==15);
  sleep(s);assert(s.order_claimed==0&&!daily_order(s,0).done);
  // New stacks, all 12 forage flags, active food and request claims survive.
  stand(s,Region::Orchard,16,12);s.gathered[0]=12;s.meals[0]=3;s.forage=0xa55;
  assert(equip_item(s,22));auto active=s.active_slot;
  assert(encode(s,bytes,sizeof(bytes))==SaveBytes&&bytes[4]==6);
  assert(decode(restored,bytes,sizeof(bytes)));
  assert(restored.gathered[0]==12&&restored.meals[0]==3&&restored.forage==0xa55&&
         restored.active_slot==active&&restored.slots[active]==22&&restored.orders_done==4);
  // A real v3 prefix migrates with its farm and resources intact.
  fresh(s);s.gold=449;s.wood=15;s.plots[0]={1,2,1,1};
  encode(s,bytes,sizeof(bytes));bytes[4]=3;
  uint32_t hash=2166136261u;for(int i=0;i<SaveBytes-4;++i)hash=(hash^bytes[i])*16777619u;
  for(int i=0;i<4;++i)bytes[SaveBytes-4+i]=hash>>(i*8);
  assert(decode(restored,bytes,sizeof(bytes))&&restored.gold==449&&restored.wood==15&&
      restored.plots[0].crop==2&&restored.orders_done==0&&restored.gathered[0]==0);
  s.gathered[0]=2;s.gathered[1]=3;s.meals[0]=1;s.fish=1;
  stand(s,Region::Farm,8,10);equip_item(s,19);assert(ship(s)==14&&s.gathered[0]==1&&s.meals[0]==1);
  assert(!change_item(s,0,1)&&!change_item(s,5,-1)&&!change_item(s,19,-2));
  puts("PASS: orchard paths, seasonal/daily forage, transactional recipes, food, gifts, four daily requests, shipping and v3 migration");
}
