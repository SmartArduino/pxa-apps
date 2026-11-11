// SPDX-License-Identifier: MIT
#include "../scene.hpp"
#include "../sound_design.hpp"
#include <cassert>
#include <cstring>
#include <cstdio>
#include <cmath>
using namespace farm;
static void canonical(const State&s,uint8_t*out){assert(encode(s,out,SaveBytes)==SaveBytes);}
static void same(const State&a,const State&b){uint8_t x[SaveBytes],y[SaveBytes];canonical(a,x);canonical(b,y);assert(!std::memcmp(x,y,sizeof(x)));}
int main() {
 State s;fresh(s);s.px=ShippingX*16+8;s.py=(ShippingY+1)*16+8;s.produce[0]=20;s.produce[2]=8;sync_inventory(s);
 assert(ship_items(s,12,20)==320&&s.produce[0]==0&&s.produce[2]==8&&s.last_shipped_count==20);
 assert(unship(s)&&s.produce[0]==20&&s.shipping==0&&!s.last_shipped_count);
 State old=s;assert(!ship_items(s,12,21));same(s,old);
 assert(ship_items(s,12,10)==160&&ship_items(s,14,3)==102&&unship(s));
 assert(s.shipping==160&&s.produce[0]==10&&s.produce[2]==8&&!unship(s));
 s.shipping=999990;old=s;assert(!ship_items(s,12,1));same(s,old);
 s.shipping=0;s.last_shipped=0;s.last_shipped_count=0;assert(ship_items(s,12,10));s.produce[0]=999;sync_inventory(s);old=s;assert(!unship(s));same(s,old);
 s.sound_effects=false;s.friend_chapter[0]=2;s.friend_chapter[1]=1;uint8_t raw[SaveBytes],packed[StorageSaveBytes];State t;
 assert(encode_storage(s,packed,sizeof(packed))==960&&decode(t,packed,sizeof(packed))&&!t.sound_effects&&t.last_shipped_count==10&&t.friend_chapter[0]==2);same(s,t);
 // A real pre-extension save has zero padding; its last deposit was one item.
 s.last_shipped_count=1;s.shipping=16;canonical(s,raw);std::memset(raw+1109,0,8);uint32_t h=2166136261u;for(int i=0;i<SaveBytes-4;++i)h=(h^raw[i])*16777619u;for(int i=0;i<4;++i)raw[SaveBytes-4+i]=h>>(8*i);
 assert(decode(t,raw,sizeof(raw))&&t.sound_effects&&t.last_shipped_count==1&&!t.friend_chapter[0]);
 fresh(s);s.region=unsigned(Region::Town);s.visited|=1u<<s.region;s.px=5*16+8;s.py=8*16+8;canonical(s,raw);std::memset(raw+1109,0,8);h=2166136261u;for(int i=0;i<SaveBytes-4;++i)h=(h^raw[i])*16777619u;for(int i=0;i<4;++i)raw[SaveBytes-4+i]=h>>(8*i);
 assert(decode(t,raw,sizeof(raw))&&walkable(t,t.px/16,t.py/16)&&t.py/16==7);
 for(int npc=0;npc<NpcCount;++npc) {
  fresh(s);s.bonds[npc]=19;assert(!friend_story(s,npc));s.bonds[npc]=20;
  assert(friend_story(s,npc)&&s.friend_chapter[npc]==1&&!friend_story(s,npc));
  s.bonds[npc]=30;assert(!friend_story(s,npc));s.story=3;for(auto &b:s.bonds)b=30;
  auto gold=s.gold;assert(friend_story(s,npc)&&s.friend_chapter[npc]==2&&s.gold==gold+100&&!friend_story(s,npc));
 }
 // Scan entire schedules: no walls/water, meaningful travel, continuity inside maps.
 for(int story:{0,5})for(int day:{1,3,14,42,70,98})for(bool rain:{false,true}) {
  fresh(s);s.story=story;s.day=day;s.rain=rain;s.weather=rain?1:0;
  unsigned places[4]{};int moving[4]{};NpcPose previous[4]{};
  for(int minute=360;minute<1560;++minute)for(int id=0;id<NpcCount;++id) {
   s.minute=minute;auto p=npc_pose(s,id,0);State probe=s;probe.region=uint8_t(p.region);
   if(!walkable(probe,p.x/16,p.y/16))std::fprintf(stderr,"bad route story=%d day=%d rain=%d minute=%d npc=%d region=%d x=%d y=%d\n",story,day,rain,minute,id,int(p.region),p.x/16,p.y/16);
   assert(walkable(probe,p.x/16,p.y/16));places[id]|=1u<<unsigned(p.region);moving[id]+=p.walking;
   if(minute>360&&previous[id].region==p.region)assert(std::abs(p.x-previous[id].x)+std::abs(p.y-previous[id].y)<=32);
   if(minute>=1320)assert(p.region==NpcHomes[id]);
   if(!story&&id!=2)assert(p.region!=Region::Lake);
   previous[id]=p;
  }
  if(!rain)for(int id=0;id<NpcCount;++id){assert(moving[id]>30);assert(__builtin_popcount(places[id])>=(!story&&id==2?2:3));}
 }
 for(const auto &sound:dewfield_audio::sounds) {
  assert(sound.count>0&&sound.count<=4&&sound.track<6);double peak=0;
  for(int i=0;i<sound.count;++i){auto &a=sound.tones[i];assert(a.frequency>=40&&a.frequency<=8000&&a.ms>=10&&a.ms<=1000&&a.attack<=a.ms&&a.release<=a.ms&&a.delay<=1000&&a.waveform<=3);peak+=std::pow(10.,a.db/20.);}
  assert(peak<.5);
 }
 constexpr int W=96,H=96,X=48,Y=75;uint16_t pixels[W*H];
 for(int prop=0;prop<3;++prop){std::memset(pixels,0,sizeof(pixels));art::Raster r{pixels,W,H};if(!prop)art::village_board(r,X,Y);else if(prop==1)art::village_bench(r,X,Y);else art::village_planter(r,X,Y,0,true);for(int y=Y;y<H;++y)for(int x=0;x<W;++x)assert(!pixels[y*W+x]);}
 puts("PASS: safe stack shipping/retrieval, legacy and 960-byte extended saves, eight friendship scenes, full-day physical NPC routes, bounded quiet sound layers and grounded village props");
}
