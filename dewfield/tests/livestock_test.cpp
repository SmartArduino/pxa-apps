// SPDX-License-Identifier: MIT
#include "../livestock.hpp"
#include "../living.hpp"
#include <cassert>
#include <cstring>
#include <cstdio>
using namespace farm;
static void day(State&s){s.trough=24;livestock_night(s);++s.day;}
static void checksum(uint8_t*b){uint32_t h=2166136261u;for(int i=0;i<(b[4]>=10?farm::SaveBytes:768)-4;++i)h=(h^b[i])*16777619u;for(int i=0;i<4;++i)b[(b[4]>=10?farm::SaveBytes:768)-4+i]=h>>(8*i);}
int main() {
 State s;fresh(s);s.px=RanchX*16+8;s.py=(RanchY+1)*16+8;
 assert(!s.ranch&&s.intro==0);begin_ranch(s);assert(animal_count(s)==2&&s.hay==20);
 begin_ranch(s);assert(animal_count(s)==2&&s.hay==20);
 assert(fill_trough(s)&&s.hay==16&&s.trough==4);auto age=s.animals[0].age;
 livestock_night(s);assert(s.animals[0].age==age+1&&s.trough==2);
 s.trough=0;age=s.animals[0].age;livestock_night(s);assert(s.animals[0].age==age&&!s.animals[0].products);
 day(s);assert(animal_stage(s.animals[0])==2&&animal_product(s.animals[0])==35);
 assert(pet_animal(s,0)&&!pet_animal(s,0));assert(collect_animal(s,0)==35&&!collect_animal(s,0));
 assert(!incubate(s,1));assert(ranch_story(s,0)&&s.ranch_chapter==1&&!ranch_story(s,0));
 day(s);assert(collect_animal(s,0)==35);assert(incubate(s,1)&&animal_reserved(s)==3);
 auto eggs=item_count(s,35);assert(!incubate(s,2)&&item_count(s,35)==eggs);
 for(int i=0;i<3;++i)day(s);
 assert(s.hatched==1&&animal_count(s)==3&&s.animals[2].age==0);
 assert(ranch_story(s,1)&&s.ranch_chapter==2);
 s.gold=9999;
 for(int species=2;species<=5;++species) {
  assert(buy_animal(s,species,false));assert(buy_animal(s,species,true));
 }
 assert(animal_count(s)==11&&animal_kinds(s)==5);
 for(auto &a:s.animals)if(a.kind)a.age=Breeds[a.kind-1].adult;
 day(s);assert(animal_reserved(s)==12);int expecting=-1;
 for(int i=0;i<AnimalMax;++i)if(s.animals[i].pregnancy)expecting=i;
 assert(expecting>=0&&s.animals[expecting].kind==3);
 assert(!buy_animal(s,1,false)&&!incubate(s,1));
 auto gestation=s.animals[expecting].pregnancy;s.trough=0;livestock_night(s);assert(s.animals[expecting].pregnancy==gestation);
 for(int i=0;i<gestation;++i)day(s);
 assert(s.born==1&&animal_count(s)==12&&s.animals[11].age==0&&s.animals[expecting].cooldown==4);
 assert(ranch_story(s,2)&&s.ranch_chapter==3);
 s.breeding=false;
 int pig=-1,sheep=-1;
 for(int i=0;i<AnimalMax;++i){if(s.animals[i].kind==5&&!s.animals[i].male)pig=i;if(s.animals[i].kind==4&&!s.animals[i].male)sheep=i;}
 assert(pig>=0&&sheep>=0);s.animals[sheep].age=8;s.day=3;day(s);
 assert(animal_product(s.animals[sheep])==32);assert(collect_animal(s,sheep)==32&&collect_animal(s,sheep)==33);
 s.animals[pig].pregnancy=1;assert(!slaughter(s,pig));s.animals[pig].pregnancy=0;
 assert(slaughter(s,pig)&&item_count(s,34)==4&&animal_count(s)==11);
 assert(!slaughter(s,pig));
 s.animal_goods[2]=2;s.animal_goods[3]=2;s.animal_goods[4]=2;sync_inventory(s);
 assert(ranch_story(s,3)&&s.ranch_chapter==4&&!ranch_story(s,3));
 assert(s.animal_goods[2]==0&&s.animal_goods[3]==0&&s.animal_goods[4]==0);
 State t;uint8_t b[SaveBytes];assert(encode(s,b,sizeof(b))==SaveBytes&&b[4]==10&&decode(t,b,b[4]>=10?sizeof(b):768));
 assert(t.born==s.born&&t.hatched==s.hatched&&t.ranch_chapter==4&&item_count(t,34)==4&&!t.breeding);
 uint8_t c[SaveBytes];encode(t,c,sizeof(c));assert(!std::memcmp(b,c,sizeof(b)));
 // A legacy farmer standing in the newly restored shed is safely moved to its front.
 t.px=RanchX*16+8;t.py=RanchY*16+8;assert(encode(t,b,sizeof(b))&&decode(t,b,b[4]>=10?sizeof(b):768));
 assert(t.px/16==RanchX&&t.py/16==RanchY+1);
 // Real v8 prefix migration bypasses the opening without creating animals.
 fresh(s);s.bait=0;sync_inventory(s);encode(s,b,sizeof(b));b[4]=8;std::memset(b+577,0,SaveBytes-581);checksum(b);
 assert(decode(t,b,b[4]>=10?sizeof(b):768)&&t.intro==IntroScenes&&!t.ranch&&!animal_count(t));
 // Range checks and reserved capacity are validated before state is replaced.
 fresh(s);begin_ranch(s);encode(s,b,sizeof(b));b[593]=6;checksum(b);assert(!decode(t,b,b[4]>=10?sizeof(b):768));
 // A full bag never removes an uncollected product or a pig.
 fresh(s);begin_ranch(s);s.px=RanchX*16+8;s.py=(RanchY+1)*16+8;
 for(int item=8;item<=28;++item)change_item(s,item,1);sync_inventory(s);
 assert(change_item(s,29,1));sync_inventory(s);assert(change_item(s,30,1));sync_inventory(s);
 s.animals[0].age=4;s.animals[0].products=5;
 s.animal_goods[6]=999;sync_inventory(s);assert(!collect_animal(s,0)&&animal_product(s.animals[0])==35);
 for(int item=29;item<=36;++item)if(!item_count(s,item))assert(change_item(s,item,1));
 sync_inventory(s);assert(equip_item(s,36)&&s.active_slot>=28);
 assert(encode(s,b,sizeof(b))&&decode(t,b,b[4]>=10?sizeof(b):768)&&t.active_slot==s.active_slot);
 for(int species=1;species<=5;++species)for(int stage=0;stage<3;++stage) {
  Animal a{uint8_t(species),0,uint8_t(stage==0?0:stage==1?Breeds[species-1].young:Breeds[species-1].adult),50};assert(animal_stage(a)==stage);
 }
 // Wool stays renewable after the displayed age reaches its saturation limit.
 fresh(s);begin_ranch(s);for(auto &a:s.animals)a=Animal{};s.animals[0]={4,0,250,80};
 int wool=0;for(int i=0;i<6;++i){day(s);collect_animal(s,0);wool+=collect_animal(s,0)==33;}
 assert(wool==2&&s.animals[0].age==250);
 puts("PASS: five species, care/feed, age forms, fertile eggs/incubation, reserved births, milk/wool, guarded pork, four ranch story chapters, full bag and v8/v9 saves");
}
