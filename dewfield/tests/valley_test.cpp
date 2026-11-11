// SPDX-License-Identifier: MIT
#include "../multiplayer.hpp"
#include "../wilderness.hpp"
#include <cassert>
#include <cstdio>
#include <cstring>
using namespace farm;
static void stand(State&s,int x,int y){s.px=x*16+8;s.py=y*16+8;}
static void sign(uint8_t*b,int n){uint32_t h=2166136261u;for(int i=0;i<n-4;++i)h=(h^b[i])*16777619u;for(int i=0;i<4;++i)b[n-4+i]=h>>(8*i);}
int main(int argc,char**argv) {
 State s,t;fresh(s);assert(s.bait==12&&!s.character_chosen);
 assert(std::abs(WellX-6)+std::abs(WellY-7)>=5);
 assert(std::abs(ShippingX-WellX)>=12);
 for(auto p:{Point{WellX,WellY},Point{ShippingX,ShippingY}}) {
  int count=0;for(auto d:{Point{0,1},Point{0,-1},Point{1,0},Point{-1,0}})count+=walkable(s,p.x+d.x,p.y+d.y);
  assert(count>=2);
 }
 // Visiting, directions and reciprocal spawns remain reachable with all rewards.
 s.story=5;s.minute=600;s.friend_chapter[1]=2;
 for(const auto&p:Portals)if(unsigned(p.to)>=unsigned(Region::SeedHome)) {
  s.region=unsigned(p.from);auto a=portal_approach(p);
  assert(walkable(s,p.x,p.y)&&walkable(s,a.x,a.y));stand(s,a.x,a.y);
  assert(travel(s,p)&&walkable(s,s.px/16,s.py/16));
  uint8_t bytes[StorageSaveBytes];assert(encode_storage(s,bytes,sizeof bytes)&&decode(t,bytes,sizeof bytes)&&t.region==s.region);
 }
 for(int id=0;id<NpcCount;++id){s.minute=360;assert(npc_pose(s,id).region==NpcHomes[id]);s.minute=1320;assert(npc_pose(s,id).region==NpcHomes[id]);}
 // No free casting, one bait per cast, every trash catch is terminal and has no fish XP.
 fresh(s);s.bait=0;sync_inventory(s);FishingSession f;f.begin(s);assert(f.no_bait&&f.phase==FishingSession::Phase::Escaped);
 s.region=unsigned(Region::SeedShop);s.minute=600;s.gold=100;assert(buy_seed(s,4,10)&&s.bait==10&&s.gold==80);
 s.region=unsigned(Region::Lake);int trash=0,fish=0;
 for(int i=0;i<1000;++i) {
  s.bait=2;sync_inventory(s);auto old=s.trash,old_xp=s.fish_caught;f.begin(s);assert(s.bait==1);
  while(f.phase==FishingSession::Phase::Waiting)f.tick(33);
  if(f.junk){assert(f.press(s)&&s.trash==old+1&&s.fish_caught==old_xp);assert(!f.press(s)&&s.trash==old+1);++trash;}
  else {assert(fish_available(s,f.species));++fish;}
 }
 assert(trash>60&&trash<200&&fish>800);
 // Stumps persist; removing one enables seeded regrowth, which pauses in winter.
 fresh(s);int i=4;auto p=WildTrees[i];s.region=unsigned(p.region);stand(s,p.x,p.y+1);s.tool=4;
 s.wild_age[i]=6;for(int day=0;day<50;++day){s.day=day+1;wild_night(s);assert(s.wild_age[i]==6);}
 assert(chop_wild(s,i)==Result::Cleared&&s.wild_age[i]==7&&s.wood==1);
 s.day=85;s.wild_age[i]=2;wild_night(s);assert(s.wild_age[i]==2);
 s.wild_age[i]=0;int stages=1;
 for(int day=1;day<80&&s.wild_age[i]<5;++day){s.day=day;wild_night(s);stages|=1<<s.wild_age[i];}
 assert(stages==63);
 // New personal appearance and inventory use the same 960-byte device budget.
 for(int gender=0;gender<2;++gender) {
  fresh(s);s.character_chosen=true;s.appearance=gender;s.bait=999;s.trash=997;sync_inventory(s);
  uint8_t bytes[StorageSaveBytes];assert(encode_storage(s,bytes,sizeof bytes)==sizeof bytes&&decode(t,bytes,sizeof bytes));
  assert(t.appearance==gender&&t.character_chosen&&t.bait==999&&t.trash==997);
  // New semantic corruption must be rejected even with a recomputed checksum.
  constexpr int appearance_at=SaveDataBytes-2-240;
  bytes[appearance_at]=2;sign(bytes,sizeof bytes);assert(!decode(t,bytes,sizeof bytes));
  dewfield_net::Personal personal;personal.capture(s);t.appearance=1-gender;personal.install(t);assert(t.appearance==gender);
  dewfield_net::Pose pose,decoded;pose.appearance=gender;uint8_t wire[dewfield_net::PoseBytes];dewfield_net::encode_pose(pose,wire);
  assert(dewfield_net::decode_pose(decoded,wire,sizeof wire)&&decoded.appearance==gender);
 }
 // Real prior-version fixtures (optional): quantities, money and day survive.
 for(int a=1;a<argc;++a){uint8_t bytes[SaveBytes];FILE*f=std::fopen(argv[a],"rb");assert(f);auto n=std::fread(bytes,1,sizeof bytes,f);std::fclose(f);
  assert(decode(t,bytes,n)&&t.day==18&&t.gold==449&&t.character_chosen&&t.bait==0&&t.appearance==0);
  assert(walkable(t,t.px/16,t.py/16));uint8_t packed[StorageSaveBytes];assert(encode_storage(t,packed,sizeof packed)&&decode(s,packed,sizeof packed));
 }
 puts("PASS: home access and schedules, courtyard spacing, bait and trash, persistent stumps, staged seasonal growth, appearance, fixed saves and legacy migration");
}
