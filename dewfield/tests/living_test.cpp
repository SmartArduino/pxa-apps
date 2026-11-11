// SPDX-License-Identifier: MIT
#include "../living.hpp"
#include <cassert>
#include <cstring>
#include <cstdio>
using namespace farm;
void stand(State&s,Region r,int x,int y){s.region=unsigned(r);s.visited|=1u<<s.region;s.px=x*16+8;s.py=y*16+8;}
void checksum(uint8_t*b){uint32_t h=2166136261u;for(int i=0;i<(b[4]>=10?farm::SaveBytes:768)-4;++i)h=(h^b[i])*16777619u;for(int i=0;i<4;++i)b[(b[4]>=10?farm::SaveBytes:768)-4+i]=h>>(i*8);}
int main(int argc,char**argv){
 State s,t;fresh(s);uint8_t b[SaveBytes];
 assert(s.slots[1]==8&&s.slots[7]==46&&!item_count(s,2));
 s.seeds[0]=1;select_slot(s,1);stand(s,Region::Farm,13,15);s.plots[plot_index(13,14)]={1,0,0,0};int e=s.energy;
 assert(act(s,13,14)==Result::Planted&&s.energy==e);assert(!s.seeds[0]&&!s.slots[1]&&s.active_slot==255&&s.tool==unsigned(Tool::Hand));
 s.produce[0]=3;s.produce[2]=7;s.fish=4;sync_inventory(s);equip_item(s,12);stand(s,Region::Farm,ShippingX,ShippingY+1);
 assert(act(s,ShippingX,ShippingY)==Result::Ship&&s.produce[0]==2&&s.produce[2]==7&&s.fish==4&&s.shipping==16);
 assert(unship(s)&&s.produce[0]==3&&s.shipping==0&&!unship(s));
 select_slot(s,-1);assert(act(s,ShippingX,ShippingY)==Result::EmptyShipping&&s.produce[0]==3);
 equip_item(s,12);assert(ship(s)==16);assert(encode(s,b,sizeof(b))==SaveBytes&&decode(t,b,b[4]>=10?sizeof(b):768)&&t.last_shipped==12&&unship(t));
 stand(s,Region::Farm,14,14);assert(!ship(s));s.energy=20;assert(eat(s,12)&&s.energy==30&&s.produce[0]==1);
 assert(!eat(s,15)&&!eat(s,17));s.produce[1]=1;assert(eat(s,13));
 s.shipping=0;s.last_shipped=0;s.energy=1;s.overwork=0;assert(spend_energy(s,3)&&s.energy==0&&s.overwork==2&&s.exhausted);
 assert(spend_energy(s,9)&&s.overwork==10&&!spend_energy(s,1));s.gold=500;stand(s,Region::Farm,14,14);sleep(s,true);assert(s.energy==100&&s.gold==450&&!s.exhausted&&!s.overwork);
 s.minute=1500;s.exhausted=false;sleep(s);assert(s.energy==100);
 s.minute=1560;s.gold=100;stand(s,Region::Home,16,18);sleep(s,true);assert(s.gold==100&&s.energy==100);
 for(int day=1;day<150;++day){
  fresh(s);s.day=day;s.minute=900;auto tomorrow=weather_on(day+1);sleep(s);assert(s.weather==unsigned(tomorrow)&&s.rain==wet_weather(tomorrow));
 }
 fresh(s);s.day=28;s.plots[0]={1,1,1,1};sleep(s);assert(season_of(s.day)==1&&!s.plots[0].crop);
 assert(crop_in_season(1,0)&&!crop_in_season(29,0)&&crop_in_season(29,2)&&crop_in_season(57,3));
 for(auto &p:Portals){fresh(s);s.story=3;s.minute=600;stand(s,p.from,p.x,p.y);assert(travel(s,p));assert(walkable(s,s.px/16,s.py/16));}
 fresh(s);s.minute=539;stand(s,Region::Town,7,8);assert(act(s,7,7)==Result::Closed);s.minute=540;assert(act(s,7,7)==Result::Travel&&s.region==unsigned(Region::SeedShop));
 s.minute=600;s.gold=100;assert(buy_seed(s,0)&&!buy_seed(s,2));s.day=3;assert(!buy_seed(s,0));s.day=1;s.minute=1020;assert(!buy_seed(s,0));
 for(int se=0;se<4;++se){fresh(s);s.day=se*28+1;int seasonal=12+se;assert(forage_available(s,seasonal));for(int other=0;other<4;++other)if(other!=se)assert(!forage_available(s,12+other));}
 for(int day=1;day<=112;++day){fresh(s);s.day=day;s.visited=255;for(int npc=0;npc<4;++npc){auto order=daily_order(s,npc);
  if(order.item>=12&&order.item<=15)assert(crop_in_season(day,order.item-12));
  if(order.item==19)assert(season_of(day)==1||season_of(day)==2);
 }}
 for(int time=360;time<1560;++time){fresh(s);s.minute=time;for(int id=0;id<4;++id){auto pose=npc_pose(s,id,250);State probe=s;probe.region=unsigned(pose.region);assert(walkable(probe,pose.x/16,pose.y/16));if(time>=1320)assert(pose.region==NpcHomes[id]);}}
 // v4 keeps its season/day, inventory order and relationship date stamps.
 fresh(s);s.bait=0;sync_inventory(s);s.day=18;s.chat_day[0]=18;s.order_day=18;s.order_claimed=1;s.gold=449;encode(s,b,sizeof(b));b[4]=4;checksum(b);
 assert(decode(t,b,b[4]>=10?sizeof(b):768)&&t.day==60&&season_of(t.day)==2&&season_day(t.day)==4&&t.chat_day[0]==60&&t.order_day==60&&t.gold==449);
 fresh(s);s.bait=0;sync_inventory(s);stand(s,Region::Home,15,7);encode(s,b,sizeof(b));b[4]=4;checksum(b);
 assert(decode(t,b,b[4]>=10?sizeof(b):768)&&walkable(t,t.px/16,t.py/16)&&t.region==s.region);
 fresh(s);stand(s,Region::Lodge,16,18);s.seasonal[4]=3;s.weather=unsigned(Weather::Snow);s.energy=0;s.overwork=6;s.exhausted=true;sync_inventory(s);encode(s,b,sizeof(b));assert(decode(t,b,b[4]>=10?sizeof(b):768)&&t.region==s.region&&t.seasonal[4]==3&&t.overwork==6&&t.exhausted);
 b[555]=255;checksum(b);assert(!decode(t,b,b[4]>=10?sizeof(b):768));
 if(argc>1) {
  FILE*f=std::fopen(argv[1],"rb");assert(f);auto n=std::fread(b,1,sizeof(b),f);std::fclose(f);
  assert(decode(t,b,n));auto migrated=t;uint8_t roundtrip[SaveBytes];encode(t,b,sizeof(b));assert(decode(t,b,b[4]>=10?sizeof(b):768));encode(t,roundtrip,sizeof(roundtrip));assert(!std::memcmp(b,roundtrip,sizeof(b)));
  assert(t.gold==migrated.gold&&!std::memcmp(t.plots,migrated.plots,sizeof(t.plots))&&!std::memcmp(t.seeds,migrated.seeds,sizeof(t.seeds)));
  std::printf("Real legacy save migrated: day=%u gold=%u region=%u\n",t.day,t.gold,t.region);
 }
 puts("PASS: individual shipping/retrieval, edible crops, empty seeds, exhaustion/rest, seasonal crops/forage, accurate forecasts, shop doors, NPC schedules and v4/v5 saves");
}
