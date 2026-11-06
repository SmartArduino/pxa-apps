// SPDX-License-Identifier: MIT
// Valid private saves plus independently composited opaque intersection pixels.
#include "../scene.hpp"
#include <cassert>
#include <cstdio>
#include <cstring>
using namespace farm_scene;
static uint16_t prop[512 * 384], person[512 * 384];
int main(int argc, char **argv) {
  assert(argc == 4);
  farm::State s;
  farm::fresh(s);
  std::memset(s.cleared, 1, sizeof(s.cleared));
  for (auto &p : s.plots)
    p = {};
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
