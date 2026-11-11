// SPDX-License-Identifier: MIT
// Valid private saves plus independently composited opaque intersection pixels.
#include "../scene.hpp"
#include "../collections.hpp"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <cstdlib>
using namespace farm_scene;
static uint16_t prop[512 * 384], person[512 * 384];
int main(int argc, char **argv) {
  assert(argc == 4);
  farm::State s;
  farm::fresh(s);s.minute=600;s.intro=farm::IntroScenes;s.character_chosen=true;
  std::memset(s.cleared, 1, sizeof(s.cleared));
  for (auto &p : s.plots)
    p = {};
  if(!std::strncmp(argv[1],"portal-",7)) {
    bool shop=!std::strcmp(argv[1]+7,"shop"),home=!std::strcmp(argv[1]+7,"home");s.region=home?unsigned(farm::Region::Home):shop?unsigned(farm::Region::SeedShop):unsigned(farm::Region::Farm);
    s.visited|=1u<<s.region;s.story=5;s.show_joystick=s.show_action=false;
    s.px=(home?16:shop?22:28)*16+8;s.py=(home?19:shop?14:7)*16+8;
    if(home){s.produce[0]=1;farm::sync_inventory(s);farm::equip_item(s,12);}
    uint8_t raw[farm::SaveBytes];assert(farm::encode(s,raw,sizeof raw));
    auto*f=std::fopen(argv[2],"wb");assert(f&&std::fwrite(raw,1,sizeof raw,f)==sizeof raw);std::fclose(f);
    f=std::fopen(argv[3],"w");assert(f);std::fputs("{}",f);std::fclose(f);return 0;
  }
  if(!std::strncmp(argv[1],"rooms-",6)) {
    s.region=unsigned(farm::Region::Home);s.visited|=1u<<s.region;s.story=5;s.gold=500;
    s.show_joystick=s.show_action=false;s.px=16*16+8;s.py=18*16+8;
    const char*mode=argv[1]+6;
    if(mode[0]>='0'&&mode[0]<='9'){
      s.region=std::atoi(mode);assert(farm::furnished(s.region));s.visited|=1u<<s.region;s.py=13*16+8;
    }
    if(!std::strcmp(mode,"cutaway")){s.px=12*16+8;s.py=11*16+8;}
    if(!std::strcmp(mode,"jamb")){s.px=15*16+8;s.py=10*16+8;}
    if(!std::strcmp(mode,"night"))s.minute=1170;
    bool legacy=!std::strcmp(argv[1]+6,"legacy");if(legacy){s.px=12*16+8;s.py=12*16+8;}
    uint8_t raw[farm::SaveBytes];assert(farm::encode(s,raw,sizeof raw));
    if(legacy){raw[farm::SaveDataBytes-7]=2;uint32_t hash=2166136261u;for(int i=0;i<farm::SaveBytes-4;++i)hash=(hash^raw[i])*16777619u;for(int i=0;i<4;++i)raw[farm::SaveBytes-4+i]=hash>>(8*i);}
    farm::State checked;assert(farm::decode(checked,raw,sizeof raw));
    auto*f=std::fopen(argv[2],"wb");assert(f&&std::fwrite(raw,1,sizeof raw,f)==sizeof raw);std::fclose(f);
    f=std::fopen(argv[3],"w");assert(f);std::fputs("{}",f);std::fclose(f);return 0;
  }
  if(!std::strncmp(argv[1],"ground-",7)) {
    const char*mode=argv[1]+7;s.story=5;s.show_joystick=s.show_action=false;
    s.px=15*16+8;s.py=14*16+8;farm::begin_ranch(s);s.ranch_chapter=6;s.trough=20;
    std::memset(s.cleared,0,sizeof(s.cleared));
    for(int i=0;i<24;++i){s.tree_age[i]=farm::Obstacles[i].kind==0?5:0;if(farm::plot_index(farm::Obstacles[i].x,farm::Obstacles[i].y)>=0)s.cleared[i]=1;}
    for(int i=0;i<farm::PlotCount;++i){int crop=i%4;s.plots[i]={1,uint8_t(crop+1),uint8_t(std::min(int(farm::Crops[crop].days),i/16)),1};}
    for(int i=0;i<5;++i){int kind=i+1;s.animals[i]={uint8_t(kind),0,farm::Breeds[i].adult,70};}
    s.animals[5]={1,0,0,60};s.animals[6]={3,0,0,60};s.animals[7]={4,0,farm::Breeds[3].young,60};
    if(!std::strcmp(mode,"rain")){s.weather=1;s.rain=true;}
    if(!std::strcmp(mode,"night"))s.minute=1170;
    if(!std::strcmp(mode,"town")){s.region=1;s.px=15*16+8;s.py=16*16+8;}
    if(!std::strcmp(mode,"lake")){s.region=3;s.px=25*16+8;s.py=11*16+8;}
    if(!std::strcmp(mode,"room")){s.region=10;s.px=16*16+8;s.py=13*16+8;}
    s.visited|=1u<<s.region;farm::sync_inventory(s);uint8_t raw[farm::SaveBytes];farm::State checked;
    assert(farm::encode(s,raw,sizeof raw)&&farm::decode(checked,raw,sizeof raw));
    auto*f=std::fopen(argv[2],"wb");assert(f&&std::fwrite(raw,1,sizeof raw,f)==sizeof raw);std::fclose(f);
    f=std::fopen(argv[3],"w");assert(f);std::fprintf(f,"{\"region\":%u}",s.region);std::fclose(f);return 0;
  }
  if(!std::strncmp(argv[1],"valley-",7)||!std::strncmp(argv[1],"settlement-",11)) {
    bool settlement=!std::strncmp(argv[1],"settlement-",11);
    const char *mode=argv[1]+(settlement?11:7);s.appearance=1;s.story=5;s.gold=500;s.show_joystick=s.show_action=false;
    for(int i=0;i<24;++i)if(farm::Obstacles[i].kind==0)s.tree_age[i]=5;
    unsigned bite=0;
    if(settlement&&(!std::strcmp(mode,"sleep")||!std::strcmp(mode,"late")||!std::strcmp(mode,"overwork"))) {
      s.day=3;s.region=unsigned(farm::Region::Home);s.energy=0;s.exhausted=true;
      s.px=10*16+8;s.py=10*16+8;s.minute=1500;
      if(!std::strcmp(mode,"late"))s.minute=1559;
      if(!std::strcmp(mode,"overwork")){s.overwork=10;s.region=0;s.px=14*16+8;s.py=14*16+8;}
    }
    else if(!std::strcmp(mode,"boy-intro")){s.appearance=0;s.intro=0;}
    else if(!std::strncmp(mode,"door-",5)) {
      int id=std::atoi(mode+5);const int xs[]={9,25,26,11},ys[]={8,19,10,19};
      s.region=id==2?3:1;s.px=xs[id]*16+8;s.py=ys[id]*16+8;
    } else if(!std::strncmp(mode,"mine-",5)){farm::enter_mine(s);farm::load_mine_floor(s,std::atoi(mode+5));}
    else if(!std::strcmp(mode,"trash")||!std::strcmp(mode,"no-bait")) {
      s.px=23*16+8;s.py=15*16+8;farm::equip_item(s,7);
      if(!std::strcmp(mode,"no-bait")){s.bait=0;farm::sync_inventory(s);}
      else for(unsigned seed=1;seed<1000;++seed){s.rng=seed;auto check=s;farm::FishingSession f;f.begin(check);if(f.junk){bite=f.bite_at;break;}}
    } else {
      s.region=std::atoi(mode);
      if(farm::indoors(s.region)){s.px=16*16+8;s.py=(settlement?13:18)*16+8;}
      else if(s.region==1){s.px=15*16+8;s.py=16*16+8;}
      else if(s.region==3){s.px=25*16+8;s.py=11*16+8;}
      if(s.region==8)farm::enter_mine(s);
    }
    s.visited|=1u<<s.region;farm::sync_inventory(s);
    uint8_t raw[farm::SaveBytes];farm::State checked;assert(farm::encode(s,raw,sizeof raw)&&farm::decode(checked,raw,sizeof raw));
    FILE*f=std::fopen(argv[2],"wb");assert(f);std::fwrite(raw,1,sizeof raw,f);std::fclose(f);
    f=std::fopen(argv[3],"w");assert(f);std::fprintf(f,"{\"bite_at\":%u}",bite);std::fclose(f);return 0;
  }
  if(!std::strcmp(argv[1],"touch-controls")) {
    s.day=2;s.story=5;s.px=10*16+8;s.py=18*16+8;s.energy=50;s.produce[2]=3;
    s.show_joystick=s.show_action=false;farm::sync_inventory(s);farm::equip_item(s,14);
    uint8_t raw[farm::SaveBytes];assert(farm::encode(s,raw,sizeof(raw)));farm::State checked;assert(farm::decode(checked,raw,sizeof(raw)));
    FILE*f=std::fopen(argv[2],"wb");assert(f&&std::fwrite(raw,1,sizeof(raw),f)==sizeof(raw));std::fclose(f);f=std::fopen(argv[3],"w");assert(f);std::fclose(f);return 0;
  }
  if(!std::strncmp(argv[1],"village-",8)) {
    const char *mode=argv[1]+8;s.story=5;s.minute=600;s.energy=50;s.gold=1200;s.show_joystick=s.show_action=false;
    if(!std::strcmp(mode,"shipping")){s.px=farm::ShippingX*16+8;s.py=(farm::ShippingY+1)*16+8;s.produce[0]=13;s.produce[2]=7;farm::sync_inventory(s);farm::equip_item(s,12);}
    else if(!std::strcmp(mode,"animals")) {farm::begin_ranch(s);for(auto &a:s.animals)a={};s.animals[0]={3,0,8,70};s.animals[1]={2,0,5,65};s.px=12*16+8;s.py=15*16+8;farm::equip_item(s,5);}
    else if(!std::strcmp(mode,"cave")){farm::enter_mine(s);s.px=16*16+8;s.py=4*16+8;}
    else {
      s.region=unsigned(farm::Region::Town);s.visited|=1u<<s.region;
      if(!std::strcmp(mode,"board")){s.px=18*16+8;s.py=11*16+8;}
      else if(!std::strcmp(mode,"rain")){s.rain=true;s.weather=1;s.px=15*16+8;s.py=12*16+8;}
      else if(!std::strcmp(mode,"route")){s.minute=480;s.px=13*16+8;s.py=11*16+8;}
      else if(!std::strcmp(mode,"festival")){s.day=14;s.minute=1080;s.px=14*16+8;s.py=16*16+8;}
      else {int id=!std::strcmp(mode,"gift")?1:0;auto pose=farm::npc_pose(s,id);s.region=unsigned(pose.region);s.visited|=1u<<s.region;s.px=pose.x;s.py=(pose.y/16+1)*16+8;farm::change_item(s,29,4);farm::sync_inventory(s);farm::equip_item(s,29);if(id==0){s.region=unsigned(farm::Region::Town);s.px=15*16+8;s.py=12*16+8;for(auto &b:s.bonds)b=30;}}
    }
    uint8_t raw[farm::SaveBytes];assert(farm::encode(s,raw,sizeof(raw)));farm::State checked;assert(farm::decode(checked,raw,sizeof(raw)));
    FILE*f=std::fopen(argv[2],"wb");assert(f&&std::fwrite(raw,1,sizeof(raw),f)==sizeof(raw));std::fclose(f);f=std::fopen(argv[3],"w");assert(f);std::fclose(f);return 0;
  }
  if(!std::strncmp(argv[1],"details-",8)) {
    const char *mode=argv[1]+8;farm::begin_ranch(s);s.story=5;s.gold=1200;s.trough=8;
    s.ranch_chapter=6;s.minute=!std::strcmp(mode,"night")?1140:600;
    s.px=farm::RanchX*16+8;s.py=(farm::RanchY+1)*16+8;
    s.show_joystick=s.show_action=false;s.mine_rewards=1;
    if(!std::strcmp(mode,"old"))s.ranch_chapter=4;
    if(!std::strcmp(mode,"repair")||!std::strcmp(mode,"lamp")) {
      int npc=!std::strcmp(mode,"repair")?1:3;
      s.ranch_chapter=npc==1?4:5;s.minute=1050;s.region=unsigned(farm::Region::Town);s.visited|=1u<<s.region;
      auto pose=farm::npc_pose(s,npc);assert(pose.region==farm::Region::Town);
      s.px=(pose.x/16)*16+8;s.py=(pose.y/16+2)*16+8;
      for(auto pair:{std::pair<int,int>{17,6},{40,4},{42,1},{33,1},{31,2}})assert(farm::change_item(s,pair.first,pair.second));
      farm::sync_inventory(s);
    }
    uint8_t raw[farm::SaveBytes];assert(farm::encode(s,raw,sizeof(raw)));farm::State checked;assert(farm::decode(checked,raw,sizeof(raw)));
    FILE*f=std::fopen(argv[2],"wb");assert(f&&std::fwrite(raw,1,sizeof(raw),f)==sizeof(raw));std::fclose(f);f=std::fopen(argv[3],"w");assert(f);std::fclose(f);return 0;
  }
  if(!std::strncmp(argv[1],"geo-",4)) {
    const char *mode=argv[1]+4;s.story=3;s.minute=600;s.show_joystick=s.show_action=false;
    for(const auto &p:farm::Portals)if(!farm::indoors(unsigned(p.from))&&!farm::indoors(unsigned(p.to))) {
      const char *area=p.from==farm::Region::Farm?"farm":p.from==farm::Region::Town?"town":p.from==farm::Region::Forest?"forest":p.from==farm::Region::Lake?"lake":"orchard";
      const char *direction[]={"north","east","south","west"};char name[48];std::snprintf(name,sizeof(name),"%s-%s",area,direction[farm::portal_heading(p)]);
      if(!std::strcmp(mode,name)){s.region=unsigned(p.from);s.visited|=1u<<s.region;auto at=farm::portal_approach(p);s.px=at.x*16+8;s.py=at.y*16+8;break;}
    }
    uint8_t raw[farm::SaveBytes];assert(farm::encode(s,raw,sizeof(raw)));farm::State checked;assert(farm::decode(checked,raw,sizeof(raw)));
    FILE*f=std::fopen(argv[2],"wb");assert(f&&std::fwrite(raw,1,sizeof(raw),f)==sizeof(raw));std::fclose(f);f=std::fopen(argv[3],"w");assert(f);std::fclose(f);return 0;
  }
  if(!std::strncmp(argv[1],"mines-",6)) {
    const char *mode=argv[1]+6;s.story=3;s.gold=500;s.energy=100;s.health=100;s.show_joystick=s.show_action=false;
    if(!std::strcmp(mode,"entry")){s.region=unsigned(farm::Region::Forest);s.visited|=1u<<s.region;s.px=22*16+8;s.py=4*16+8;farm::equip_item(s,6);}
    else if(!std::strcmp(mode,"forest")) {s.region=unsigned(farm::Region::Forest);s.visited|=1u<<s.region;s.px=17*16+8;s.py=5*16+8;s.tool_grade[4]=1;farm::equip_item(s,5);}
    else {
      farm::enter_mine(s,1);s.sword_grade=1;s.tool_grade[5]=1;
      int floor=!std::strcmp(mode,"gallery-iron")?11:!std::strcmp(mode,"gallery-guardian")?30:!std::strcmp(mode,"iron")?11:!std::strcmp(mode,"crystal")?21:!std::strcmp(mode,"guardian")?30:1;
      farm::load_mine_floor(s,floor);s.invulnerable=0;
      if(!std::strcmp(mode,"ore")) {s.px=5*16+8;s.py=5*16+8;farm::equip_item(s,6);for(int i=0;i<farm::MineMobs;++i){s.mobs[i].hp=0;s.mine_defeated[0]|=1u<<i;}}
      else if(!std::strcmp(mode,"forage")){s.px=5*16+8;s.py=7*16+8;farm::equip_item(s,4);for(int i=0;i<farm::MineMobs;++i){s.mobs[i].hp=0;s.mine_defeated[0]|=1u<<i;}}
      else if(!std::strcmp(mode,"combat")){s.px=16*16+8;s.py=6*16+8;s.sword_grade=2;for(int i=0;i<farm::MineMobs;++i){s.mobs[i].hp=0;s.mine_defeated[0]|=1u<<i;}s.mobs[1]={s.px,uint16_t(7*16+8),2000,0,0,10,0};s.mine_defeated[0]&=~2u;farm::equip_item(s,37);}
      else if(!std::strcmp(mode,"stairs")){s.px=25*16+8;s.py=18*16+8;s.mine_cleared[0]|=7|farm::MineLadder;farm::equip_item(s,37);}
      else {s.px=16*16+8;s.py=10*16+8;farm::equip_item(s,37);}
      if(!std::strcmp(mode,"guardian")){s.px=25*16+8;s.py=18*16+8;}
      if(!std::strcmp(mode,"gallery-iron")) {
        for(auto &m:s.mobs)m.hp=0;s.mine_defeated[s.mine_depth-1]=63;s.monsters_slain=6;farm::check_ladder(s);
      }
      if(!std::strncmp(mode,"gallery-",8)) {
        s.px=(!std::strcmp(mode,"gallery-guardian")?25:7)*16+8;s.py=19*16+8;
        if(!std::strcmp(mode,"gallery-bag")) {
          for(int item=12;item<=45;++item)if(item!=37)farm::change_item(s,item,4);
          farm::sync_inventory(s);farm::equip_item(s,37);
        }
      }
    }
    uint8_t bytes[farm::SaveBytes];assert(farm::encode(s,bytes,sizeof(bytes))==sizeof(bytes));farm::State checked;assert(farm::decode(checked,bytes,sizeof(bytes)));
    FILE*f=std::fopen(argv[2],"wb");assert(f&&std::fwrite(bytes,1,sizeof(bytes),f)==sizeof(bytes));std::fclose(f);f=std::fopen(argv[3],"w");assert(f);std::fclose(f);return 0;
  }
  if(!std::strncmp(argv[1],"livestock-",10)) {
    farm::begin_ranch(s);s.gold=2500;s.hay=30;s.trough=24;s.ranch_chapter=1;
    s.px=farm::RanchX*16+8;s.py=(farm::RanchY+1)*16+8;s.show_joystick=s.show_action=false;
    for(int i=0;i<10;++i) {
      int kind=i/2+1;s.animals[i]={uint8_t(kind),uint8_t(i%2),farm::Breeds[kind-1].adult,65};
    }
    s.animals[0].products=5;s.animals[2].products=1;s.animals[3].age=3;
    s.animals[4].products=1;s.animals[5].age=0;s.animals[6].products=3;s.animals[7].age=3;
    s.animals[8].pregnancy=1;s.incubators[0]={1,1};farm::select_slot(s,-1);
    if(!std::strcmp(argv[1],"livestock-gallery")) {
      for(auto &a:s.animals)a={};
      for(int i=0;i<5;++i){s.animals[i]={uint8_t(i+1),0,farm::Breeds[i].adult,65};s.animals[i+6]={uint8_t(i+1),1,uint8_t(farm::Breeds[i].adult/2),50};}
      s.px=15*16+8;s.py=16*16+8;
    }
    if(!std::strcmp(argv[1],"livestock-slow")){s.auto_run=false;s.px=15*16+8;s.py=14*16+8;}
    if(!std::strcmp(argv[1],"livestock-night")){s.region=unsigned(farm::Region::Home);s.visited|=1u<<s.region;s.px=10*16+8;s.py=10*16+8;}
    uint8_t bytes[farm::SaveBytes];assert(farm::encode(s,bytes,sizeof(bytes))==sizeof(bytes));farm::State t;assert(farm::decode(t,bytes,sizeof(bytes)));
    FILE*f=std::fopen(argv[2],"wb");assert(f&&std::fwrite(bytes,1,sizeof(bytes),f)==sizeof(bytes));std::fclose(f);f=std::fopen(argv[3],"w");assert(f);std::fclose(f);return 0;
  }
  if(!std::strncmp(argv[1],"actors-",7)) {
    const char *mode=argv[1]+7;s.story=5;s.visited=255;s.day=12;s.minute=1110;s.energy=80;
    s.px=15*16+8;s.py=11*16+8;
    if(!std::strcmp(mode,"villagers")) {
      s.region=unsigned(farm::Region::Town);s.day=40;s.minute=1030;s.met=15;
      for(auto &bond:s.bonds)bond=10;
      s.show_joystick=s.show_action=false;farm::select_slot(s,-1);
    }
    else {
      s.minute=600;int d=std::atoi(mode);assert(d>=0&&d<4);int dx=(d==1)-(d==3),dy=(d==2)-(d==0);
      s.px=(19-dx)*16+8;s.py=(14-dy)*16+8;s.cleared[7]=0;s.tree_age[7]=5;s.tool_grade[4]=1;
      farm::select_slot(s,4);
    }
    uint8_t bytes[farm::SaveBytes];assert(farm::encode(s,bytes,sizeof(bytes))==sizeof(bytes));
    farm::State checked;assert(farm::decode(checked,bytes,sizeof(bytes)));
    FILE*f=std::fopen(argv[2],"wb");assert(f&&std::fwrite(bytes,1,sizeof(bytes),f)==sizeof(bytes));std::fclose(f);
    f=std::fopen(argv[3],"w");assert(f);std::fclose(f);return 0;
  }
  if(!std::strncmp(argv[1],"discoveries-",12)) {
    const char *mode=argv[1]+12;s.day=12;s.minute=600;s.energy=80;s.harvested=10;
    s.px=23*16+8;s.py=15*16+8;
    farm::change_item(s,12,3);farm::change_item(s,20,2);
    farm::sync_inventory(s);farm::select_slot(s,6);
    if(!std::strcmp(mode,"catalog")) {
      s.story=3;s.visited=255;
      for(int item:farm::GardenCollection)farm::change_item(s,item,1);
      for(int item:farm::WildCollection)farm::change_item(s,item,1);
      for(int i=0;i<farm::FishSpeciesCount;++i)s.fish_records[i]=farm::FishSpeciesList[i].max_cm;
      s.fish_caught=25;s.perfect_catches=3;
    }
    uint8_t bytes[farm::SaveBytes];assert(farm::encode(s,bytes,sizeof(bytes))==sizeof(bytes));
    farm::State checked;assert(farm::decode(checked,bytes,sizeof(bytes)));
    FILE*f=std::fopen(argv[2],"wb");assert(f&&std::fwrite(bytes,1,sizeof(bytes),f)==sizeof(bytes));std::fclose(f);
    farm::FishingSession session;session.begin(s);
    f=std::fopen(argv[3],"w");assert(f);
    std::fprintf(f,"{\"bite_at\":%u,\"center\":%d,\"cycle_ms\":%u,\"species\":%d}\n",session.bite_at,session.center,session.cycle_ms,session.species);
    std::fclose(f);return 0;
  }
  if(!std::strncmp(argv[1],"redesign-",9)) {
    const char *mode=argv[1]+9;s.story=3;s.visited=255;s.day=12;s.minute=600;s.energy=60;
    s.px=14*16+8;s.py=14*16+8;
    if(!std::strcmp(mode,"door")){s.px=5*16+8;s.py=7*16+8;}
    if(!std::strcmp(mode,"route")){s.px=9*16+8;s.py=10*16+8;}
    if(!std::strcmp(mode,"hud-left")){s.region=1;s.px=2*16+8;s.py=2*16+8;}
    if(!std::strcmp(mode,"hud-right")){s.region=3;s.px=28*16+8;s.py=2*16+8;}
    if(!std::strcmp(mode,"hud-belt")){s.px=14*16+8;s.py=22*16+8;}
    if(!std::strcmp(mode,"home")){s.region=4;s.px=264;s.py=200;s.show_joystick=s.show_action=false;}
    if(!std::strcmp(mode,"shop")){s.region=6;s.px=264;s.py=200;s.show_joystick=s.show_action=false;}
    if(!std::strcmp(mode,"forest")){s.region=2;s.px=248;s.py=200;}
    if(!std::strcmp(mode,"lake")){s.region=3;s.px=392;s.py=200;}
    if(!std::strcmp(mode,"town")){s.region=1;s.px=248;s.py=200;}
    if(!std::strcmp(mode,"soil"))for(int y=0;y<8;++y)for(int x=0;x<10;++x)
      if(y%3!=2)s.plots[y*10+x]={1,uint8_t(x<5?1:2),uint8_t(x%4==0?0:x<5?2:3),uint8_t(x>=5)};
    farm::select_slot(s,-1);
    if(!std::strcmp(mode,"bag")) {
      s.px=farm::ShippingX*16+8;s.py=(farm::ShippingY+1)*16+8;
      s.produce[0]=3;farm::sync_inventory(s);
      for(int i=0;i<farm::InventorySize;++i)if(s.slots[i]==12){std::swap(s.slots[i],s.slots[27]);break;}
    }
    uint8_t bytes[farm::SaveBytes];assert(farm::encode(s,bytes,sizeof(bytes))==sizeof(bytes));
    FILE*f=std::fopen(argv[2],"wb");assert(f&&std::fwrite(bytes,1,sizeof(bytes),f)==sizeof(bytes));std::fclose(f);
    f=std::fopen(argv[3],"w");assert(f);std::fclose(f);return 0;
  }
  if (!std::strncmp(argv[1],"atmosphere-",11)) {
    s.minute=std::atoi(argv[1]+11);s.day=12;s.px=232;s.py=194;
    for(int y=0;y<4;++y)for(int x=0;x<10;++x)
      if(y!=2)s.plots[y*10+x]={1,uint8_t(x<5?1:2),uint8_t(x<5?2:3),1};
    uint8_t bytes[farm::SaveBytes];assert(farm::encode(s,bytes,sizeof(bytes))==sizeof(bytes));
    FILE*f=std::fopen(argv[2],"wb");assert(f&&std::fwrite(bytes,1,sizeof(bytes),f)==sizeof(bytes));std::fclose(f);
    f=std::fopen(argv[3],"w");assert(f);std::fclose(f);return 0;
  }
  if(!std::strncmp(argv[1],"living-",7)) {
    const char *mode=argv[1]+7;s.visited=255;s.story=3;s.day=29;s.minute=600;s.gold=480;s.energy=60;
    s.region=!std::strcmp(mode,"home")?4:!std::strcmp(mode,"shop")?6:(!std::strcmp(mode,"town")||!std::strcmp(mode,"npc"))?1:!std::strcmp(mode,"lodge")?7:!std::strcmp(mode,"orchard")?5:0;
    s.px=(s.region==4?15:s.region==6?15:s.region==1?15:s.region==7?16:s.region==5?12:8)*16+8;
    s.py=(s.region==4?9:s.region==6?11:s.region==1?12:s.region==7?15:s.region==5?11:10)*16+8;
    if(!std::strcmp(mode,"npc")){s.minute=561;s.px=15*16+8;s.py=15*16+8;}
    if(!std::strcmp(mode,"night"))s.minute=1320;
    if(!std::strcmp(mode,"snow")){s.day=85;s.weather=3;s.minute=900;}
    for(auto &n:s.produce)n=4;for(auto &n:s.seasonal)n=3;s.fish=4;s.gathered[0]=8;
    farm::sync_inventory(s);farm::equip_item(s,12);
    uint8_t bytes[farm::SaveBytes];assert(farm::encode(s,bytes,sizeof(bytes))==sizeof(bytes));farm::State checked;assert(farm::decode(checked,bytes,sizeof(bytes)));
    FILE*f=std::fopen(argv[2],"wb");assert(f&&std::fwrite(bytes,1,sizeof(bytes),f)==sizeof(bytes));std::fclose(f);
    f=std::fopen(argv[3],"w");assert(f);std::fclose(f);return 0;
  }
  if (!std::strncmp(argv[1], "life-", 5)) {
    s.region = !std::strcmp(argv[1],"life-orchard") ? 5 :
               !std::strcmp(argv[1],"life-kitchen") ? 4 : 1;
    s.day=29;s.story=3;s.visited=63;s.gold=480;s.energy=60;
    s.px=(s.region==5?12:s.region==4?21:18)*16+8;
    s.py=11*16+8;s.wood=s.stone=s.fish=20;
    for(auto&v:s.produce)v=20;
    s.gathered[0]=8;s.gathered[1]=6;s.gathered[2]=5;s.meals[0]=s.meals[1]=1;
    farm::sync_inventory(s);farm::select_slot(s,3);
    uint8_t bytes[farm::SaveBytes];assert(farm::encode(s,bytes,sizeof(bytes))==sizeof(bytes));
    farm::State checked;assert(farm::decode(checked,bytes,sizeof(bytes)));
    FILE*f=std::fopen(argv[2],"wb");assert(f&&std::fwrite(bytes,1,sizeof(bytes),f)==sizeof(bytes));std::fclose(f);
    f=std::fopen(argv[3],"w");assert(f);std::fclose(f);return 0;
  }
  if (!std::strncmp(argv[1], "world-", 6)) {
    s.region=!std::strcmp(argv[1],"world-forest")?2:!std::strcmp(argv[1],"world-lake")?3:!std::strcmp(argv[1],"world-home")?4:1;
    s.visited=31;s.story=s.region==2?2:s.region==3?3:0;s.met=15;
    s.minute=s.region==2?720:600;s.harvested=3;s.wood=12;s.stone=9;s.fish=3;s.produce[0]=4;
    if(!std::strcmp(argv[1],"world-festival")){s.story=5;s.minute=1080;for(auto &v:s.bonds)v=10;}
    s.px=(s.region==2?16:s.region==3?24:s.region==4?16:15)*16+8;s.py=(s.region==4?17:12)*16+8;
    uint8_t bytes[farm::SaveBytes];assert(farm::encode(s,bytes,sizeof(bytes))==sizeof(bytes));farm::State checked;assert(farm::decode(checked,bytes,sizeof(bytes)));
    FILE *f=std::fopen(argv[2],"wb");assert(f&&std::fwrite(bytes,1,sizeof(bytes),f)==sizeof(bytes));std::fclose(f);
    f=std::fopen(argv[3],"w");assert(f);std::fclose(f);return 0;
  }
  if (!std::strncmp(argv[1], "tool-", 5)) {
    bool axe = !std::strcmp(argv[1], "tool-axe");
    bool rod = !std::strcmp(argv[1], "tool-rod");
    int id = axe ? 0 : 1;
    s.cleared[id] = 0;
    s.tree_age[0] = 5;
    s.px = (rod ? 23 : farm::Obstacles[id].x - 1) * 16 + 8;
    s.py = (rod ? 15 : farm::Obstacles[id].y) * 16 + 8;
    farm::select_slot(s, rod ? 6 : axe ? 4 : 5);
    s.tool_grade[4] = s.tool_grade[5] = 1;
    uint8_t bytes[farm::SaveBytes];
    assert(farm::encode(s, bytes, sizeof(bytes)) == sizeof(bytes));
    FILE *f = std::fopen(argv[2], "wb");
    assert(f && std::fwrite(bytes, 1, sizeof(bytes), f) == sizeof(bytes));
    std::fclose(f);
    f = std::fopen(argv[3], "w"); assert(f); std::fclose(f);
    return 0;
  }
  if (!std::strcmp(argv[1], "cultivation")) {
    for (int y = 0; y < farm::PlotH; ++y)
      for (int x = 0; x < farm::PlotW; ++x) {
        auto &p = s.plots[y * farm::PlotW + x];
        p = {1, 0, 0, uint8_t(x >= 5)};
        if (y % 2 == 0 && x % 5 < 4) {
          p.crop = y / 2 + 1;
          int days = farm::Crops[p.crop - 1].days;
          p.growth = x % 5 == 0 ? 0 : x % 5 == 1 ? 1 :
                     x % 5 == 2 ? days - 1 : days;
        }
      }
    s.px = 232;
    s.py = 194;
    uint8_t bytes[farm::SaveBytes];
    assert(farm::encode(s, bytes, sizeof(bytes)) == sizeof(bytes));
    farm::State checked;
    assert(farm::decode(checked, bytes, sizeof(bytes)));
    FILE *f = std::fopen(argv[2], "wb");
    assert(f && std::fwrite(bytes, 1, sizeof(bytes), f) == sizeof(bytes));
    std::fclose(f);
    f = std::fopen(argv[3], "w"); assert(f); std::fclose(f);
    return 0;
  }
  Kind kind;
  int x, y, depth, id = 0, actorx;
  if (std::strstr(argv[1], "chest")) {
    kind = Kind::Chest;
    x = 136;
    y = depth = 156;
    actorx = 150;
  } else if (std::strstr(argv[1], "well")) {
    kind = Kind::Well;
    x = 120;
    y = depth = 140;
    actorx = 104;
  } else if (std::strstr(argv[1], "house")) {
    kind = Kind::House;
    x = 64;
    y = 48;
    depth = 109;
    actorx = 156;
  } else if (std::strstr(argv[1], "villager")) {
    kind = Kind::Villager;
    x = 357;
    y = depth = 155;
    actorx = 360;
  } else if (std::strstr(argv[1], "crop")) {
    kind = Kind::Crop;
    x = 168;
    y = depth = soil::root_y(30);
    actorx = 168;
    id = 30;
    s.plots[30] = {1, 1, 2, 1};
  } else {
    kind = Kind::Sprinkler;
    x = 232;
    y = depth = 239;
    actorx = 232;
    id = 54;
    s.sprinklers[id / 8] |= 1u << (id % 8);
  }
  bool front = std::strstr(argv[1], "front");
  s.px = actorx;
  s.py = depth + (front ? 3 : -3);
  if (kind == Kind::House)
    s.py = front ? 111 : 107;
  uint8_t bytes[farm::SaveBytes];
  assert(farm::encode(s, bytes, sizeof(bytes)) == sizeof(bytes));
  farm::State checked;
  assert(farm::decode(checked, bytes, sizeof(bytes)));
  FILE *f = std::fopen(argv[2], "wb");
  assert(f);
  assert(std::fwrite(bytes, 1, sizeof(bytes), f) == sizeof(bytes));
  std::fclose(f);
  Queue q;
  View v{};
  v.held_item = s.slots[s.active_slot];
  v.tool = s.tool;
  q.add(kind, x, y, depth, id);
  art::Raster r{prop, 512, 384};
  q.objects(r, s, v);
  Queue actor;
  actor.add(Kind::Player, s.px, s.py, s.py);
  r.p = person;
  actor.objects(r, s, v);
  f = std::fopen(argv[3], "w");
  assert(f);
  int count = 0;
  for (int yy = 0; yy < 384; ++yy)
    for (int xx = 0; xx < 512; ++xx) {
      int at = yy * 512 + xx;
      if (prop[at] && person[at]) {
        std::fprintf(f, "%d,%d,%u\n", xx, yy,
                     unsigned(front ? person[at] : prop[at]));
        ++count;
      }
    }
  std::fclose(f);
  assert(count > 4);
  std::printf("%s %u %u %d\n", argv[1], unsigned(s.px), unsigned(s.py), count);
}
