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
  // Both directions travel the same 12-pixel stretch in 1.6 s. Feet advance
  // with distance, rather than flapping during an almost stationary glide.
  // All poses stay in their original pasture cell; care targeting is stable.
  int phase=(int(s.minute)*500+ms+id*1279)%16000;
  bool west=phase>=8000;int t=phase%8000,off=6,frame=0;
  if(t<1600){off=-6+t*12/1600;frame=1+(t*8/1600);}
  else if(t>=2600&&t<6600){
    // Grazing/pecking is visual; daily feed/product accounting stays in night().
    int eat=(t-2600)%2000;
    constexpr int poses[]={9,10,11,12,13,12,13,14,10,9};
    frame=poses[eat/200];
  }
  return {(10+id%6*2)*16+8+(west?-off:off),(18*16+13)+(id/6)*25,west?3:1,frame};
}
inline int animal_at(const State&s,int x,int y) {
  if(s.region)return -1;
  for(int i=0;i<AnimalMax;++i)if(s.animals[i].kind) {auto p=animal_pose(s,i);if(p.x/16==x&&p.y/16==y)return i;}
  return -1;
}
inline constexpr int RanchStoryChapters=6;
inline int ranch_contact(const State&s) {
  constexpr int contacts[]={0,1,2,3,1,3};
  return s.ranch&&s.ranch_chapter<RanchStoryChapters?contacts[s.ranch_chapter]:-1;
}
inline bool ranch_lit(const State&s) {return s.ranch&&s.ranch_chapter>=6&&(s.minute>=1080||s.minute<420);}
inline const char *ranch_goal(const State&s,bool en=false) {
  const char *cn[]={"小禾：带一枚鸡蛋来镇上","阿岚：孵化第一只幼鸟","澄：养三种动物，迎来一只幼兽","婆婆：牛奶羊奶羊毛各2份","阿岚：木材6、铁矿4，加固旧棚","婆婆：晶石1、羊毛1、牛奶2","屋檐暖灯：每晚等你回家"};
  const char *eng[]={"Bring an egg to Xiaohe","Hatch a bird, then visit Alan","Raise 3 species and a newborn; visit Cheng","Bring 2 milk, 2 sheep milk, 2 wool to Lin","Alan: 6 wood and 4 iron ore for the shed","Lin: 1 crystal, 1 wool and 2 milk","A warm light welcomes you home each night"};
  if(s.ranch_chapter==5&&!(s.mine_rewards&1))return en?"Mine floor 30: recover the lamp crystal heart":"矿洞三十层：找回矿灯晶心";
  return (en?eng:cn)[std::min(RanchStoryChapters,int(s.ranch_chapter))];
}
inline const char *ranch_prompt(const State&s,bool en=false) {
  if(s.ranch_chapter==4)return en?"Alan: Rain found the old roof joints. Bring 6 wood and 4 iron ore; I'll fit new braces and hang a lantern.":"阿岚：上次下雨，旧棚梁缝漏了水。带来木材6份、铁矿4份，我替小家伙们加固屋檐，再挂一盏灯。";
  if(s.ranch_chapter==5)return en?((s.mine_rewards&1)?"Lin: The old lamp can shine again. Bring 1 crystal, 1 wool and 2 milk. A blanket, warm milk and a light to come home to.":"Lin: The lamp's heart sleeps on mine floor 30. Find it first; then bring 1 crystal, 1 wool and 2 milk."):
    ((s.mine_rewards&1)?"林婆婆：旧灯罩擦亮了。再带晶石1份、羊毛1份、牛奶2份吧。织条小毯，煮好热奶，让屋檐下的灯等你归来。":"林婆婆：老矿灯的晶心还在矿洞三十层。找回它，再带晶石1份、羊毛1份、牛奶2份，我们一起点亮旧棚。" );
  return ranch_goal(s,en);
}
inline const char *ranch_completion(int chapter,bool en=false) {
  const char *cn[]={"小禾：第一枚蛋里有整个清晨的耐心。旧孵化器修好了！受精鸡蛋等3晚，鸭蛋等4晚，记得给幼鸟留个空位。", "阿岚：小家伙破壳时，我想起第一次搭好房梁。成年公母吃饱、关系良好，也能一起迎接幼崽。这些金币送给你。", "澄：一个涟漪，也能让湖面热闹起来。你让旧农场重新有了生命。再为大家准备一份清晨的礼物吧。", "林婆婆：羊毛织新毯，牛奶摆上餐桌。童年的春天，被你一天天种回来了。阿岚还想看看旧棚的屋梁。", "阿岚：新木撑住旧梁，铁箍扣紧屋檐。以后听见雨声，小家伙们也能安心睡了。灯笼挂好了，找婆婆问问旧灯的事吧。", "林婆婆：晶心亮起来了，热奶的香气留在羊毛毯里。傍晚六点，屋檐下会亮一盏灯。再晚归来，也有一个地方等着你。"};
  const char *eng[]={"Xiaohe: I repaired the incubator! Fertile chicken eggs hatch after 3 nights; duck eggs need 4. Keep a stall free for the youngster.", "Alan: That first hatch reminds me of raising my first roof. Here are some coins. A well-fed, happy adult pair can welcome a baby.", "Cheng: A single ripple can fill a lake with life. Your farm is alive again. Let's prepare a morning gift for everyone.", "Lin: Wool for blankets, milk for our table. You planted spring here again. Alan wants to check the old shed beams.", "Alan: New wood and iron braces keep the rain out. The lantern is hung. Ask Lin about bringing its old light back.", "Lin: The crystal glows; warm milk scents the wool blanket. At six each evening, a light under the eaves will welcome you home."};
  return (en?eng:cn)[std::clamp(chapter-1,0,RanchStoryChapters-1)];
}
inline bool ranch_story_ready(const State&s,int npc) {
  if(npc<0||npc!=ranch_contact(s))return false;
  switch(s.ranch_chapter) {
    case 0:return item_count(s,29)||item_count(s,35);
    case 1:return s.hatched;
    case 2:return animal_kinds(s)>=3&&s.born;
    case 3:return item_count(s,31)>=2&&item_count(s,32)>=2&&item_count(s,33)>=2;
    case 4:return item_count(s,17)>=6&&item_count(s,40)>=4;
    case 5:return (s.mine_rewards&1)&&item_count(s,42)>=1&&item_count(s,33)>=1&&item_count(s,31)>=2;
    default:return false;
  }
}
inline bool ranch_story(State&s,int npc) {
  if(!ranch_story_ready(s,npc))return false;
  switch(s.ranch_chapter) {
    case 0:change_item(s,item_count(s,29)?29:35,-1);break;
    case 3:for(int item:{31,32,33})change_item(s,item,-2);break;
    case 4:change_item(s,17,-6);change_item(s,40,-4);break;
    case 5:change_item(s,42,-1);change_item(s,33,-1);change_item(s,31,-2);break;
  }
  ++s.ranch_chapter;s.gold=std::min(999999u,s.gold+100u*s.ranch_chapter);s.bonds[npc]=std::min(40,int(s.bonds[npc])+5);sync_inventory(s);return true;
}
}
