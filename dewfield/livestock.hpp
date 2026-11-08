// SPDX-License-Identifier: MIT
#pragma once
#include "core.hpp"
#include <algorithm>
namespace farm {
inline constexpr int RanchX=21,RanchY=12;
struct Breed {const char *cn,*en;uint16_t price;uint8_t young,adult,gestation;};
inline constexpr Breed Breeds[]={
  {"鸡","Chicken",80,2,4,0},{"鸭","Duck",120,3,5,0},
  {"牛","Cow",380,3,7,4},{"羊","Sheep",300,3,6,3},{"猪","Pig",240,2,5,3}};
inline const char *animal_good(int item,bool en=false) {
  const char *cn[]={"鸡蛋","鸭蛋","牛奶","羊奶","羊毛","猪肉","受精鸡蛋","受精鸭蛋"};
  const char *eng[]={"Egg","Duck egg","Milk","Sheep milk","Wool","Pork","Fertile egg","Fertile duck egg"};
  return (en?eng:cn)[std::clamp(item-29,0,7)];
}
inline int animal_stage(const Animal&a) {if(!a.kind)return 0;auto &b=Breeds[a.kind-1];return a.age<b.young?0:a.age<b.adult?1:2;}
inline int animal_count(const State&s) {int n=0;for(auto &a:s.animals)n+=a.kind!=0;return n;}
inline int animal_reserved(const State&s) {int n=animal_count(s);for(auto &a:s.animals)n+=a.pregnancy!=0;for(auto &e:s.incubators)n+=e.kind!=0;return n;}
inline int animal_kinds(const State&s) {unsigned mask=0;for(auto &a:s.animals)if(a.kind)mask|=1u<<a.kind;return __builtin_popcount(mask);}
inline int free_animal(const State&s) {for(int i=0;i<AnimalMax;++i)if(!s.animals[i].kind)return i;return -1;}
inline bool ranch_here(const State&s) {return !s.region&&near(s,RanchX,RanchY);}
inline void begin_ranch(State&s) {
  if(s.ranch)return;
  s.ranch=1;s.hay=20;
  s.animals[0]={1,0,2,55};s.animals[1]={1,1,2,55};
  // Clear only the pasture, keeping crop obstacles and the rest of the farm.
  for(int i=0;i<24;++i)if(Obstacles[i].x>=10&&Obstacles[i].x<=22&&Obstacles[i].y>=18) {
    s.cleared[i]=1;s.tree_age[i]=Obstacles[i].kind==0?5:0;s.damage[i]=0;
  }
}
inline bool buy_animal(State&s,int species,bool male) {
  if(!ranch_here(s)||!s.ranch||species<1||species>5||animal_reserved(s)>=AnimalMax)return false;
  auto cost=Breeds[species-1].price;if(s.gold<cost)return false;
  int i=free_animal(s);if(i<0)return false;
  s.gold-=cost;s.animals[i]={uint8_t(species),uint8_t(male),0,50};return true;
}
inline bool buy_hay(State&s) {if(!ranch_here(s)||s.gold<20||s.hay>989)return false;s.gold-=20;s.hay+=10;return true;}
inline bool fill_trough(State&s) {
  if(!ranch_here(s))return false;
  int n=std::min(int(s.hay),std::max(0,animal_count(s)*2-int(s.trough)));
  if(!n)return false;s.hay-=n;s.trough+=n;return true;
}
inline bool pet_animal(State&s,int id) {
  if(id<0||id>=AnimalMax||!s.animals[id].kind||s.animals[id].pet_day==s.day)return false;
  auto &a=s.animals[id];a.pet_day=s.day;a.heart=std::min(100,int(a.heart)+5);return true;
}
inline int animal_product(const Animal&a) {
  if(!a.kind)return 0;
  if(a.products&1)return a.kind<=2?((a.products&4)?34+a.kind:28+a.kind):a.kind==3?31:32;
  return a.products&2?33:0;
}
inline int collect_animal(State&s,int id) {
  if(id<0||id>=AnimalMax)return 0;auto &a=s.animals[id];int item=animal_product(a);
  if(!item||!change_item(s,item,1))return 0;
  if(a.products&1)a.products&=~5u;else a.products&=~2u;
  s.animal_collected=std::min(65535,int(s.animal_collected)+1);sync_inventory(s);return item;
}
inline bool incubate(State&s,int kind) {
  if(!ranch_here(s)||s.ranch_chapter<1||kind<1||kind>2||animal_reserved(s)>=AnimalMax)return false;
  for(auto &e:s.incubators)if(!e.kind) {
    if(!change_item(s,34+kind,-1))return false;
    e={uint8_t(kind),uint8_t(kind==1?3:4)};sync_inventory(s);return true;
  }
  return false;
}
inline bool slaughter(State&s,int id) {
  if(s.region||id<0||id>=AnimalMax)return false;
  if(!ranch_here(s)&&!near(s,10+id%6*2,(18*16+13+(id/6)*25)/16))return false;auto &a=s.animals[id];
  if(a.kind!=5||animal_stage(a)!=2||a.pregnancy||!change_item(s,34,4))return false;
  a=Animal{};sync_inventory(s);return true;
}
inline void livestock_night(State&s) {
  if(!s.ranch)return;
  bool fed[AnimalMax]{};uint8_t births[AnimalMax]{};
  // Births use capacity reserved at conception/incubation, never silently drop.
  for(int i=0;i<AnimalMax;++i) {
    auto &a=s.animals[i];if(!a.kind)continue;
    if(s.trough) {--s.trough;fed[i]=true;a.fed_day=s.day;a.age=std::min(250,int(a.age)+1);a.heart=std::min(100,int(a.heart)+4);}
    else {a.heart=std::max(0,int(a.heart)-8);continue;}
    if(a.cooldown)--a.cooldown;
    if(a.pregnancy&&!--a.pregnancy) {
      births[i]=a.kind;a.cooldown=4;
    }
  }
  for(auto kind:births)if(kind) {
    int slot=free_animal(s);
    if(slot>=0){s.animals[slot]={kind,uint8_t(random(s)&1),0,55};s.born=std::min(65535,int(s.born)+1);}
  }
  for(auto &e:s.incubators)if(e.kind) {
    if(e.days>1)--e.days;
    else {int slot=free_animal(s);if(slot>=0){s.animals[slot]={e.kind,uint8_t(random(s)&1),0,55};e={};s.hatched=std::min(65535,int(s.hatched)+1);}}
  }
  for(int i=0;i<AnimalMax;++i) {
    auto &a=s.animals[i];if(!a.kind||!fed[i]||animal_stage(a)<2||a.heart<30)continue;
    bool partner=false;
    for(int j=0;j<AnimalMax;++j)if(j!=i&&fed[j]&&s.animals[j].kind==a.kind&&s.animals[j].male!=a.male&&animal_stage(s.animals[j])==2&&s.animals[j].heart>=40)partner=true;
    if(a.kind<=2&&!a.male&&!(a.products&1))a.products|=1|(partner?4:0);
    if((a.kind==3||a.kind==4)&&!a.male)a.products|=1;
    if(a.kind==4&&s.day%3==0)a.products|=2;
    if(a.kind>=3&&!a.male&&partner&&s.breeding&&a.heart>=40&&!a.pregnancy&&!a.cooldown&&animal_reserved(s)<AnimalMax)
      a.pregnancy=Breeds[a.kind-1].gestation;
  }
}
struct AnimalPose {int x,y,heading,frame;};
inline AnimalPose animal_pose(const State&s,int id,int ms=0) {
  int phase=(int(s.minute)*500+ms+id*743)%10000;
  int off=phase<2400?phase*6/2400:phase<5000?6:phase<7400?(7400-phase)*6/2400:0;
  bool moving=phase<2400||(phase>=5000&&phase<7400);
  return {(10+id%6*2)*16+8+off,(18*16+13)+(id/6)*25,phase<5000?1:3,moving?(phase/180)%2:0};
}
inline int animal_at(const State&s,int x,int y) {
  if(s.region)return -1;
  for(int i=0;i<AnimalMax;++i)if(s.animals[i].kind) {auto p=animal_pose(s,i);if(p.x/16==x&&p.y/16==y)return i;}
  return -1;
}
inline const char *ranch_goal(const State&s,bool en=false) {
  const char *cn[]={"小禾：带一枚鸡蛋来镇上","阿岚：孵化第一只幼鸟","澄：养三种动物，迎来一只幼兽","婆婆：牛奶羊奶羊毛各2份","牧场餐桌：每个清晨有新声音"};
  const char *eng[]={"Bring an egg to Xiaohe","Hatch a bird, then visit Alan","Raise 3 species and a newborn; visit Cheng","Bring 2 milk, 2 sheep milk, 2 wool to Lin","A lively farm, a shared morning table"};
  return (en?eng:cn)[std::min(4,int(s.ranch_chapter))];
}
inline const char *ranch_completion(int chapter,bool en=false) {
  const char *cn[]={"小禾：第一枚蛋里有整个清晨的耐心。我修好了旧孵化器！放入受精鸡蛋等3晚，鸭蛋等4晚，就能听见新的叫声。", "阿岚：小家伙破壳的那一刻，我也想起第一次搭起房梁。送你一些金币。成年公母吃饱、关系良好，就能一起迎接幼崽。", "澄：湖面也是这样，一开始只有一个涟漪，后来满是生命。你已经让旧农场重新热闹起来了。再为大家准备一份清晨的礼物吧。", "林婆婆：羊毛替会馆织新毯，牛奶和羊奶一起摆上餐桌。原来童年的春天不是留在过去，而是被你一天天种回来了。"};
  const char *eng[]={"Xiaohe: I repaired the incubator! Fertile chicken eggs hatch after 3 nights; duck eggs need 4. Keep a stall free for the youngster.", "Alan: That first hatch reminds me of raising my first roof. Here are some coins. A well-fed, happy adult pair can welcome a baby.", "Cheng: A single ripple can fill a lake with life. Your farm is alive again. Let's prepare a morning gift for everyone.", "Lin: Wool for the lodge blankets, milk for our morning table. You didn't leave your childhood spring behind. You planted it here, one day at a time."};
  return (en?eng:cn)[std::clamp(chapter-1,0,3)];
}
inline bool ranch_story(State&s,int npc) {
  if(!s.ranch||s.ranch_chapter>=4||npc!=int(s.ranch_chapter))return false;
  if(!s.ranch_chapter) {
    int item=item_count(s,29)?29:35;if(!change_item(s,item,-1))return false;
  } else if(s.ranch_chapter==1) {if(!s.hatched)return false;}
  else if(s.ranch_chapter==2) {if(animal_kinds(s)<3||!s.born)return false;}
  else {if(s.animal_goods[2]<2||s.animal_goods[3]<2||s.animal_goods[4]<2)return false;for(int i:{2,3,4})s.animal_goods[i]-=2;}
  ++s.ranch_chapter;s.gold=std::min(999999u,s.gold+100u*s.ranch_chapter);s.bonds[npc]=std::min(40,int(s.bonds[npc])+5);sync_inventory(s);return true;
}
}
