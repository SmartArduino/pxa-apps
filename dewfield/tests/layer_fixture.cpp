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
  farm::fresh(s);s.minute=600;
  std::memset(s.cleared, 1, sizeof(s.cleared));
  for (auto &p : s.plots)
    p = {};
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
      s.px=8*16+8;s.py=10*16+8;
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
