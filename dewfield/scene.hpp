// SPDX-License-Identifier: MIT
#pragma once
#include "art.hpp"
#include "core.hpp"
#include "living.hpp"
#include "settlement.hpp"
#include "wilderness.hpp"
#include "mines.hpp"
#include <algorithm>
#include <cstdint>
namespace farm_scene {
enum class Kind : uint8_t {
  Tree,
  Rock,
  Grass,
  House,
  Well,
  Chest,
  Board,
  Fence,
  Crop,
  Sprinkler,
  Villager,
  Chicken,
  Player,
  WorldNpc,
  RemotePlayer,
  Bed,
  Table,
  Forage,
  Stove, Television, Calendar, Counter, Cabinet, Fireplace, Chair, Shelf, Signpost, Animal, RanchShed, WildTree, MineEntrance, Ore, Monster, CaveCrop, MineLadder, MineTorch, VillageBoard, Bench, Planter, Workbench, Bookcase, WoodRack, Divider, Armchair, KitchenCounter, RoomWall, RoomPost, RoomDoor
};
struct Rect {
  int x, y, w, h;
  bool intersects(const Rect &b) const {
    return x < b.x + b.w && x + w > b.x && y < b.y + b.h && y + h > b.y;
  }
};
struct Entry {
  Kind kind;
  int16_t x, y, depth;
  uint16_t order;
  uint8_t id;
  bool interactive;
  uint8_t growth = 2;
  uint8_t facing = 2;
};
struct RemoteFarmer {
  int x=0,y=0,direction=2,frame=0,swing=0,tool=3,item=0,grade=0,seed=0,number=1;
  art::tool_anim::Target target{};
  int appearance=0;
};
struct View {
  int direction = 2, frame = 0, swing = 0, tool = 0, season = 0;
  uint32_t animation = 0;
  int held_item = 0, grade = 0, seed = 0, hit_id = -1;
  uint32_t hit_ms = 0;
  bool fishing = false;
  int subminute_ms=0;
  art::tool_anim::Target tool_target{};
  const RemoteFarmer *farmers=nullptr;
  unsigned farmer_count=0;
};
struct Placement {
  const sprites::Sprite *sprite = nullptr;
  int x = 0, y = 0;
};
inline const furniture_sprites::Image* furniture_image(const Entry&e) {
  using namespace furniture_sprites;
  switch(e.kind){
  case Kind::Bed:return &Bed;case Kind::Table:return &Table;case Kind::Chair:return ChairDirections[e.facing&3];
  case Kind::Cabinet:return CabinetDirections[e.facing&3];case Kind::Stove:return &Stove;case Kind::Fireplace:return &Fireplace;
  case Kind::Workbench:return &Workbench;case Kind::Bookcase:return &Bookcase;case Kind::Divider:return &Divider;
  case Kind::Armchair:return ArmchairDirections[e.facing&3];case Kind::WoodRack:return e.id==2?&Barrels:&WoodRack;
  case Kind::Television:return &Television;case Kind::Calendar:return &Calendar;
  case Kind::Bench:return &Bench;case Kind::Planter:return &Planter;default:return nullptr;
  case Kind::KitchenCounter:return &CounterBay;
  }
}
// One physical foot anchor feeds culling, painting, contact and picking.
// Houses keep their map origin while their front step is the ground anchor.
inline Placement placement(const Entry &e) {
  if(auto*f=furniture_image(e))return {&f->shape,e.x,e.y};
  switch (e.kind) {
  case Kind::WorldNpc:
    return {&art::villager_walking_sprite(e.id % 4, e.id / 4, e.growth), e.x,
            e.y};
  case Kind::Forage: {
    if (farm::Foraging[e.id].item >= 19) return {};
    const sprites::Sprite *items[] = {&sprites::logs, &sprites::stones,
                                      &sprites::fish, &sprites::seeds};
    auto &f = farm::Foraging[e.id];
    auto *s = items[f.item == 17 ? 0 : f.item == 18 ? 1 : f.item == 16 ? 2 : 3];
    return {s, e.x - s->w / 2, e.y - s->h};
  }
  case Kind::WildTree:return {&art::tree_sprite(e.x,e.y,0,e.growth,farm::WildTrees[e.id].variant),e.x,e.y};
  case Kind::Tree:
    return {&art::tree_sprite(e.x, e.y, 0, e.growth, e.id == 255 ? 3 : -1), e.x, e.y};
  case Kind::Rock:
    return {&art::rock_sprite(e.x, e.y), e.x, e.y};
  case Kind::Grass:
    return {&art::grass_sprite(e.x, e.y), e.x, e.y};
  case Kind::House:
    return {&art::building_shape(e.id), e.x + 40, e.y + 61};
  case Kind::Well:
    return {&sprites::well, e.x, e.y};
  case Kind::Chest:
    return {&sprites::chest, e.x, e.y};
  case Kind::Board:
    return {&sprites::board, e.x, e.y};
  case Kind::Sprinkler:
    return {&sprites::sprinkler, e.x, e.y};
  case Kind::Villager:
    return {&sprites::villager, e.x, e.y};
  default:
    return {};
  }
}
inline Rect bounds(const Entry &e) {
  if(e.kind==Kind::RoomDoor)return {e.x-2,e.y-farm::walls::Rise,5,76};
  if(e.kind==Kind::RoomPost)return {e.x-6,e.y-6,12,12};
  if(e.kind==Kind::RoomWall){int overhang=e.id&farm::walls::DoorCorner?farm::walls::ReturnRise:0;
    return {e.x*16,e.y*16-farm::walls::Rise-overhang,16,farm::walls::ImageHeight+overhang};}
  if(e.kind==Kind::KitchenCounter)return {e.x-23,e.y-36,47,38};
  if(e.kind==Kind::Workbench)return {e.x-34,e.y-35,69,37};
  if(e.kind==Kind::Bookcase)return {e.x-35,e.y-46,71,48};
  if(e.kind==Kind::WoodRack)return {e.x-24,e.y-39,49,41};
  if(e.kind==Kind::Divider)return {e.x-37,e.y-26,75,28};
  if(e.kind==Kind::Armchair)return {e.x-16,e.y-32,33,34};
  if(e.kind==Kind::MineEntrance)return {e.x-27,e.y-46,55,51};
  if(e.kind==Kind::Ore)return {e.x-14,e.y-24,28,26};
  if(e.kind==Kind::Monster)return {e.x-17,e.y-30,35,32};
  if(e.kind==Kind::CaveCrop)return {e.x-8,e.y-17,17,19};
  if(e.kind==Kind::MineLadder||e.kind==Kind::MineTorch)return {e.x-12,e.y-24,25,27};
  if(e.kind==Kind::Animal)return {e.x-16,e.y-32,32,33};
  if(e.kind==Kind::RanchShed)return {e.x-32,e.y-50,65,51};
  if(e.kind==Kind::Counter)return {e.x-3,e.y-44,182,47};
  if(e.kind==Kind::Television)return {e.x-16,e.y-38,33,40};
  if(e.kind==Kind::VillageBoard)return {e.x-29,e.y-45,59,46};
  if(e.kind==Kind::Bench)return {e.x-19,e.y-25,39,26};
  if(e.kind==Kind::Planter)return {e.x-14,e.y-22,29,23};
  if(e.kind==Kind::Calendar)return {e.x-13,e.y-32,27,34};
  if(e.kind==Kind::Cabinet)return {e.x-20,e.y-35,40,37};
  if(e.kind==Kind::Fireplace)return {e.x-22,e.y-46,44,48};
  if(e.kind==Kind::Chair)return {e.x-11,e.y-26,22,28};
  if(e.kind==Kind::Shelf)return {e.x-130,e.y-37,260,40};
  if(e.kind==Kind::Signpost)return {e.x-13,e.y-28,27,30};
  auto p = placement(e);
  if (p.sprite)
    return {p.x - p.sprite->anchor_x - 1, p.y - p.sprite->anchor_y,
            p.sprite->w + 2, p.sprite->h + 2};
  switch (e.kind) {
  case Kind::Bed:
    return {e.x - 20, e.y - 46, 40, 48};
  case Kind::Stove:
    return {e.x - 24, e.y - 50, 48, 52};
  case Kind::Forage:
    return {e.x - 8, e.y - 14, 17, 15};
  case Kind::Table:
    return {e.x - 22, e.y - 29, 44, 31};
  case Kind::Fence:
    return {e.x - 1, e.y, 53, 17};
  case Kind::Crop:
    return {e.x - 11, e.y - 22, 24, 25};
  case Kind::Chicken:
    return {e.x - 4, e.y - 12, 13, 14};
  case Kind::RemotePlayer:
  case Kind::Player:
    return {e.x - 30, e.y - 60, 64, 84};
  default:
    return {0, 0, 0, 0};
  }
}
inline bool faded(const Entry &e, const farm::State &s) {
  if ((e.kind != Kind::Tree&&e.kind!=Kind::WildTree) || s.py >= e.depth)
    return false;
  auto p = placement(e);
  int item = s.active_slot < farm::InventorySize ? s.slots[s.active_slot] : 0;
  bool carrying = farm::carrying_item(item);
  int height = carrying ? 54 : 28;
  return Rect{int(s.px) - 10, int(s.py) - height, 20, height - 4}.intersects(
      {p.x - p.sprite->anchor_x + 1, p.y - p.sprite->anchor_y + 1,
       p.sprite->w - 2, p.sprite->h - std::max(3, int(p.sprite->h) / 3)});
}
struct Queue {
  static constexpr unsigned Capacity = 292;
  Entry entries[Capacity]{};
  unsigned count = 0;
  Rect viewport{0, 0, farm::MapW * 16, farm::MapH * 16};
  void add(Kind kind, int x, int y, int depth, int id = 0,
           bool interactive = false, int growth = 2,int facing=2) {
    Entry e{kind,
            int16_t(x),
            int16_t(y),
            int16_t(depth),
            uint16_t(next_order++),
            uint8_t(id),
            interactive,
            uint8_t(growth),uint8_t(facing)};
    auto visible=bounds(e);visible.w+=8;visible.h+=8;
    if (!visible.intersects(viewport))
      return;
    // Capacity covers even a completely planted and irrigated full map.
    if (count >= Capacity)
      __builtin_trap();
    entries[count++] = e;
  }
  void sort() {
    std::sort(entries, entries + count, [](const Entry &a, const Entry &b) {
      return a.depth != b.depth ? a.depth < b.depth : a.order < b.order;
    });
  }
  // Merge at most three remote farmers into the already sorted scene queue.
  // Keep the normal map at one sort per frame.
  void remote_farmers(const View&v){
    for(unsigned i=0;i<v.farmer_count;++i){
      unsigned previous=count;auto&p=v.farmers[i];add(Kind::RemotePlayer,p.x,p.y,p.y,int(i));
      if(count==previous)continue;
      auto entry=entries[count-1];unsigned at=count-1;
      while(at&&entries[at-1].depth>entry.depth){entries[at]=entries[at-1];--at;}
      entries[at]=entry;
    }
  }
  void signs(const farm::State&s) {
    if(farm::indoors(s.region))return;
    for(unsigned i=0;i<sizeof(farm::Portals)/sizeof(*farm::Portals);++i) {
      const auto&p=farm::Portals[i];
      if(unsigned(p.from)!=s.region||farm::indoors(uint8_t(p.to)))continue;
      int d=farm::portal_heading(p),x=p.x*16+8+(d%2?0:16),y=p.y*16+12+(d%2?16:0);
      add(Kind::Signpost,x,y,y,i,true);
    }
  }
  void build(const farm::State &s, const View &v, Rect clip) {
    count = 0;
    next_order = 0;
    viewport = clip;
    if (s.region) {
      build_region(s, v);
      sort();
      return;
    }
    // Stable tie ordering is shared by rendering and picking.
    for (int y : {8 * 16, 17 * 16 + 7})
      for (int x : {10 * 16, 16 * 16})
        add(Kind::Fence, x, y, y + 15);
    for (int y : {14, 21})
      for (int x = 24; x < 30; x += 2) {
        add(Kind::Rock, x * 16, y * 16, y * 16);
        add(Kind::Grass, x * 16 + 10, y * 16 + 2, y * 16 + 2);
      }
    for (int x = 1; x < farm::MapW; x += 2) {
      if(!farm::exit_clearance(s.region,x,1))add(Kind::Tree, x * 16, 29, 29, x);
      if(!farm::exit_clearance(s.region,x,22))add(Kind::Tree, x * 16, farm::MapH * 16 - 2, farm::MapH * 16 - 2, x + 2);
    }
    for (int y = 4; y < farm::MapH - 2; y += 3) {
      if(!farm::exit_clearance(s.region,0,y))add(Kind::Tree, 14, y * 16, y * 16, y);
      if(!farm::exit_clearance(s.region,31,y))add(Kind::Tree, farm::MapW * 16 - 8, y * 16, y * 16, y + 1);
    }
    const auto &cottage=farm::Buildings[0];
    add(Kind::House,cottage.x*16,cottage.y*16,cottage.y*16+61,cottage.kind,true);
    // The farm has a home; the seed store lives in town.
    for(int x:{24,27})add(Kind::Grass,x*16+8,5*16+13,5*16+13);
    add(Kind::Well, farm::WellX * 16 + 8, farm::WellY * 16 + 12, farm::WellY * 16 + 12, 0, true);
    add(Kind::Chest, farm::ShippingX * 16 + 8, farm::ShippingY * 16 + 12, farm::ShippingY * 16 + 12, 0, true);
    add(Kind::Board, 21 * 16 + 8, 8 * 16 + 12, 8 * 16 + 12, 0, true);
    for (int i = 0; i < farm::PlotCount; ++i) {
      int x = (farm::PlotX + i % farm::PlotW) * 16 + 8,
          y = (farm::PlotY + i / farm::PlotW) * 16;
      if (s.plots[i].crop)
        add(Kind::Crop, soil::root_x(i), soil::root_y(i), soil::root_y(i), i);
      if (s.sprinklers[i / 8] & (1u << (i % 8)))
        add(Kind::Sprinkler, x, y + 15, y + 15, i);
    }
    for (int i = 0; i < 24; ++i) {
      auto &o = farm::Obstacles[i];
      int plot = farm::plot_index(o.x, o.y);
      bool stump = o.kind == 0 && s.cleared[i] && s.tree_age[i] < 2 &&
                   (plot < 0 || !s.plots[plot].tilled);
      if (s.cleared[i] && !stump)
        continue;
      int x = o.x * 16 + 8, y = o.y * 16 + (o.kind ? 12 : 13);
      add(o.kind == 0   ? Kind::Tree
          : o.kind == 1 ? Kind::Rock
                        : Kind::Grass,
          x, y, y, i, true, o.kind == 0 ? farm::tree_stage(s, i) : 2);
    }
    for(int id=0;id<4;++id){auto p=farm::npc_pose(s,id,v.subminute_ms);if(p.region==farm::Region::Farm)
      add(Kind::WorldNpc,p.x,p.y,p.y,id+p.dir*4,true,p.frame);
    }
    add(Kind::RanchShed,farm::RanchX*16+8,farm::RanchY*16+12,farm::RanchY*16+12,0,true);
    for(int id=0;id<farm::AnimalMax;++id)if(s.animals[id].kind) {
      auto p=farm::animal_pose(s,id,v.subminute_ms);
      add(Kind::Animal,p.x,p.y,p.y,id,true,(p.heading==3?128:0)+p.frame);
    }
    signs(s);
    add(Kind::Player, s.px, s.py, s.py);
    sort();
  }
  void build_region(const farm::State &s, const View &v) {
    using farm::Region;
    unsigned door_phases[2]={};auto doors=farm::walls::door_count(s.region);
    auto near_doors=[&](int x,int y){for(unsigned i=0;i<doors;++i)door_phases[i]=std::max(door_phases[i],farm::walls::door_open(farm::walls::RoomDoors[i],x,y));};
    near_doors(s.px,s.py);
    for(unsigned i=0;i<v.farmer_count;++i)near_doors(v.farmers[i].x,v.farmers[i].y);
    if(farm::in_mine(s)) {
      for(int i=0;i<farm::MineRocks;++i){auto p=farm::ore_tile(i);if(!(s.mine_cleared[s.mine_depth-1]&(1u<<i)))add(Kind::Ore,p.x*16+8,p.y*16+13,p.y*16+13,i,true);}
      for(int i=0;i<farm::MineMobs;++i)if(s.mobs[i].hp){auto &m=s.mobs[i];add(Kind::Monster,m.x,m.y,m.y,i,true);}
      for(int i=0;i<4;++i){auto p=farm::cave_forage_tile(i);if(farm::cave_forage_at(s,p.x,p.y)>=0)add(Kind::CaveCrop,p.x*16+8,p.y*16+13,p.y*16+13,i,true);}
      add(Kind::MineLadder,16*16+8,2*16+12,2*16+12,0,true);
      add(Kind::MineLadder,25*16+8,17*16+12,17*16+12,1,true);
      for(int y:{10,19})for(int x:{4,28})add(Kind::MineTorch,x*16+8,y*16+12,y*16+12);
      add(Kind::Player,s.px,s.py,s.py);return;
    }
    if (farm::indoors(s.region)) {
      farm::walls::PostSite posts[24];unsigned post_count=farm::walls::shell_posts(s.region,posts);
      for(unsigned i=0;i<post_count;++i)add(Kind::RoomPost,posts[i].x,posts[i].y,posts[i].depth);
      if(farm::partitioned(s.region)){
        for(int y=5;y<=12;++y)for(int x=7;x<=24;++x)if(farm::partition_wall(s.region,x,y))
          add(Kind::RoomWall,x,y,y*16+farm::walls::Front,farm::walls::links(s.region,x,y));
      }
      if(s.region==uint8_t(Region::SeedShop)) {
        add(Kind::Counter,11*16,10*16+15,10*16+15,0,true);
        add(Kind::Shelf,16*16+8,7*16+15,7*16+15);
        for(int xx:{8,23})add(Kind::Planter,xx*16+8,16*16+15,16*16+15);
      } else if(farm::furnished(s.region)) {
        constexpr Kind kinds[]={Kind::Bed,Kind::Cabinet,Kind::Stove,Kind::Table,Kind::Chair,Kind::Fireplace,Kind::Planter,Kind::Workbench,Kind::Bookcase,Kind::WoodRack,Kind::Divider,Kind::Armchair,Kind::Television,Kind::Calendar,Kind::KitchenCounter};
        int owner=s.region>=uint8_t(Region::SeedHome)?s.region-uint8_t(Region::SeedHome):0;
        for(auto p:farm::home_plan(s.region)){
          int y=p.y*16+(p.kind==farm::HomeProp::Calendar?1:15);
          bool interactive=s.region==uint8_t(Region::Home)&&(p.kind==farm::HomeProp::Bed||p.kind==farm::HomeProp::Stove||p.kind==farm::HomeProp::Television||p.kind==farm::HomeProp::Calendar);
          int offset=p.kind==farm::HomeProp::Cabinet?(p.facing==3?8:p.facing==1?-8:0):0;
          add(kinds[unsigned(p.kind)],p.x*16+8+offset,y,y,owner,interactive,2,p.facing);
        }
      }
    } else {
      for (int x = 1; x < 32; x += 2) {
        if(!farm::exit_clearance(s.region,x,1))add(Kind::Tree, x * 16, 29, 29);
        if(!farm::exit_clearance(s.region,x,22))add(Kind::Tree, x * 16, 382, 382);
      }
      for (int y = 4; y < 22; y += 3) {
        if (y != 12) {
          if(!farm::exit_clearance(s.region,0,y))add(Kind::Tree, 14, y * 16, y * 16);
          if(!farm::exit_clearance(s.region,31,y))add(Kind::Tree, 504, y * 16, y * 16);
        }
      }
      for(const auto &site:farm::Buildings)if(unsigned(site.area)==s.region)add(Kind::House,site.x*16,site.y*16,site.y*16+61,site.kind,true);
      if (s.region == uint8_t(Region::Town)) {
        if (s.story >= 5)
          add(Kind::Table, 15 * 16 + 8, 15 * 16, 15 * 16);
        for(int xx:{5,10})add(Kind::Planter,xx*16+8,8*16+12,8*16+12);
        add(Kind::Bench,11*16+8,10*16+12,10*16+12);
        add(Kind::Well, 16 * 16 + 8, 10 * 16 + 12, 10 * 16 + 12);
        add(Kind::VillageBoard,18*16+8,10*16+12,10*16+12,0,true);
        for (int x : {19}) {
          add(Kind::Grass, x * 16, 15 * 16, 15 * 16);
          add(Kind::Grass, x * 16, 9 * 16, 9 * 16);
        }
        for (int x : {18})
          add(Kind::Fence, x * 16, 16 * 16, 16 * 16 + 15);
      } else if(s.region==uint8_t(Region::Lake)) {
        if(s.friend_chapter[1]>=2)add(Kind::Bench,26*16+8,14*16+12,14*16+12);
      } else if (s.region == uint8_t(Region::Forest)) {
        add(Kind::MineEntrance,22*16+8,3*16+12,3*16+12,0,true);
        for (int i = 0; i < 9; ++i) {
          int x=5+i*3,y=i%2?17:8;
          if(farm::world_ground(s,x,y)!=farm::Ground::Water)
            add(Kind::Grass,x*16+8,y*16+12,y*16+12);
        }
      } else if (s.region == uint8_t(Region::Orchard)) {

        for (int x : {5, 11, 19, 25})
          add(Kind::Fence, x * 16, 19 * 16, 19 * 16 + 15);
        for (int x : {8, 21, 28})
          add(Kind::Grass, x * 16, 14 * 16, 14 * 16);
      }
    }
    for(int i=0;i<farm::WildTreeCount;++i){auto &t=farm::WildTrees[i];if(s.wild_age[i]==7)continue;if(unsigned(t.region)==s.region)add(Kind::WildTree,t.x*16+8,t.y*16+13,t.y*16+13,i,true,farm::wild_stage(s,i));}
    for (unsigned i = 0; i < sizeof(farm::Foraging) / sizeof(*farm::Foraging);
         ++i) {
      auto &f = farm::Foraging[i];
      if (unsigned(f.region) == s.region && farm::forage_available(s, i))
        add(Kind::Forage, f.x * 16 + 8, f.y * 16 + 13, f.y * 16 + 13, i, true);
    }
    for (int id = 0; id < 4; ++id) {
      auto p = farm::npc_pose(s, id,v.subminute_ms);
      if (unsigned(p.region) == s.region){
        near_doors(p.x,p.y);
        add(Kind::WorldNpc, p.x, p.y, p.y, id + p.dir * 4, true,
            p.frame);
      }
    }
    for(unsigned i=0;i<doors;++i){auto d=farm::walls::RoomDoors[i];
      add(Kind::RoomDoor,d.x,d.y,d.y+48,i,false,door_phases[i]);
    }
    signs(s);
    add(Kind::Player, s.px, s.py, s.py);
    (void)v;
  }
  void shadows(art::Raster &r, const farm::State &s, const View &v = {}) const {
    // Soft body/canopy casts and exact root/sole contacts are separate. Both
    // sample the same ground, so crossings do not multiply into black patches.
    if(r.shadow_ground)for(unsigned i=0;i<count;++i) {
      const auto&e=entries[i];int width=0,rows=4,x=e.x,y=e.y;
      switch(e.kind) {
      case Kind::House:{auto p=placement(e);art::foundation_shadow(r,*p.sprite,p.x,p.y);continue;}
      case Kind::Tree:case Kind::WildTree:
        width=e.growth==0?3:e.growth==1?15:e.growth==3?9:28;
        rows=e.growth==2?8:e.growth==1?5:3;break;
      case Kind::Rock:case Kind::Ore:width=19;rows=5;break;
      case Kind::Grass:case Kind::CaveCrop:width=8;rows=3;break;
      case Kind::Chest:width=23;rows=6;break;
      case Kind::Well:width=31;rows=7;break;
      case Kind::RanchShed:width=54;rows=6;break;
      case Kind::Sprinkler:width=7;rows=3;break;
      case Kind::Board:case Kind::VillageBoard:
        for(int dx:{-17,17})art::soft_ground_shadow(r,x+dx,y,6,4);continue;
      case Kind::Bench:
        for(int dx:{-13,13})art::soft_ground_shadow(r,x+dx,y,6,4);continue;
      case Kind::Fence:
        for(int dx=0;dx<=48;dx+=16)art::soft_ground_shadow(r,x+dx+1,y+15,5,4);continue;
      case Kind::Planter:width=22;rows=5;break;
      case Kind::Animal:{
        const auto&a=s.animals[e.id];int stage=farm::animal_stage(a);
        width=a.kind>=3?14+stage*5:7+stage*3;rows=3+stage;break;
      }
      case Kind::Player:case Kind::RemotePlayer:case Kind::WorldNpc:case Kind::Villager:width=10;rows=3;break;
      case Kind::Chicken:width=8;rows=3;break;
      case Kind::Crop:{const auto&p=s.plots[e.id];width=p.growth?9:4;rows=p.growth?4:2;break;}
      case Kind::Forage:width=10;rows=3;break;
      case Kind::Bed:width=33;break;
      case Kind::Table:width=35;break;
      case Kind::Workbench:case Kind::Bookcase:case Kind::Divider:width=62;break;
      case Kind::WoodRack:width=36;break;
      case Kind::Armchair:width=28;break;
      case Kind::Stove:case Kind::Fireplace:width=37;break;
      case Kind::Cabinet:width=furniture_image(e)->shape.w-4;break;
      case Kind::KitchenCounter:width=41;break;
      case Kind::Chair:width=16;rows=3;break;
      case Kind::Counter:width=173;x+=86;break;
      case Kind::Shelf:width=244;break;
      case Kind::Signpost:case Kind::MineTorch:width=6;rows=3;break;
      case Kind::Monster:width=17;rows=4;break;
      case Kind::MineEntrance:width=47;rows=3;break;
      // Flat ladders and wall-mounted calendars/TVs keep only their contact.
      default:continue;
      }
      art::soft_ground_shadow(r,x+1,y,width,rows);
    }
    for (unsigned i = 0; i < count; ++i) {
      const auto &e = entries[i];
      switch (e.kind) {
      case Kind::RoomDoor:art::room_door_shadow(r,e.x,e.y,e.growth);break;
      case Kind::WildTree:{auto p=placement(e);art::contact_shadow(r,*p.sprite,p.x,p.y);break;}
      case Kind::Ore:art::contact_shadow(r,art::rock_sprite(e.x,e.y),e.x,e.y);break;
      case Kind::Monster:art::mine_monster_shadow(r,e.x,e.y,farm::mob_type(s,e.id));break;
      case Kind::MineEntrance:art::shadow_strip(r,e.x-25,e.y,51,1);break;
      case Kind::CaveCrop:art::shadow_strip(r,e.x-3,e.y,7,1);break;
      case Kind::MineLadder:case Kind::MineTorch:break;
      case Kind::VillageBoard:art::shadow_strip(r,e.x-18,e.y,3,2);art::shadow_strip(r,e.x+16,e.y,3,2);break;
      case Kind::Animal:art::animal_shadow(r,e.x,e.y,s.animals[e.id],e.growth&128?3:1,e.growth&127);break;
      case Kind::RanchShed:art::shadow_strip(r,e.x-22,e.y,46,1);break;
      case Kind::WorldNpc:
        art::gait::shadow(r,e.x,e.y,e.id/4,e.growth,&art::villager_rig(e.id%4,e.id/4));
        break;
      case Kind::Counter:
        for(int bay=0;bay<4;++bay)art::contact_shadow(r,furniture_sprites::CounterBay.shape,e.x+22+bay*44,e.y);break;
      case Kind::Shelf:
        for(int bay=0;bay<4;++bay)art::contact_shadow(r,furniture_sprites::ShelfBay.shape,e.x-96+bay*64,e.y);break;
      case Kind::Signpost:art::shadow_strip(r,e.x-2,e.y,5,1);break;
      case Kind::Player: {
        art::gait::AppearanceGuard appearance(s.appearance);
        art::person_shadow(r, e.x, e.y, v.direction, v.frame);
        break; }
      case Kind::RemotePlayer: {
        auto&p=v.farmers[e.id];art::gait::AppearanceGuard appearance(p.appearance);art::person_shadow(r,e.x,e.y,p.direction,p.frame);break;
      }
      case Kind::Chicken:
        art::chicken_shadow(r, e.x, e.y, v.animation / 2 + e.id);
        break;
      case Kind::Crop: {
        const auto &p = s.plots[e.id];
        const auto &c = farm::Crops[p.crop - 1];
        int stage = p.growth == 0        ? 0
                    : p.growth >= c.days ? 3
                                         : p.growth * 3 / c.days;
        art::crop_shadow(r, e.x, e.y, p.crop - 1, stage, p.growth >= c.days);
        break;
      }
      case Kind::Fence:
        art::fence_shadow(r, e.x, e.y, 48);
        break;
      case Kind::Forage:
        if (farm::Foraging[e.id].item >= 19) art::shadow_strip(r,e.x-5,e.y,11,1);
        else {auto p=placement(e);art::contact_shadow(r,*p.sprite,p.x,p.y);}
        break;
      default: {
        auto p = placement(e);
        if (p.sprite)
          art::contact_shadow(r, *p.sprite, p.x, p.y);
        break;
      }
      }
    }
  }
  void objects(art::Raster &r, const farm::State &s, const View &v) const {
    for (unsigned i = 0; i < count; ++i) {
      auto &e = entries[i];
      switch (e.kind) {
      case Kind::RoomDoor:art::room_door(r,e.x,e.y,e.growth);break;
      case Kind::RoomWall:art::room_wall(r,e.x,e.y,e.id,s.px,s.py);break;
      case Kind::RoomPost:art::room_post(r,e.x,e.y,s.py<e.depth&&std::abs(int(s.px)-e.x)<16&&std::abs(int(s.py)-20-e.y)<20?3:8);break;
      case Kind::Workbench:art::room_workbench(r,e.x,e.y,e.id);break;
      case Kind::Bookcase:art::room_bookcase(r,e.x,e.y,e.id);break;
      case Kind::WoodRack:art::room_woodrack(r,e.x,e.y,e.id);break;
      case Kind::Divider:art::room_divider(r,e.x,e.y,e.id);break;
      case Kind::Armchair:art::furniture(r,*furniture_image(e),e.x,e.y);break;
      case Kind::WildTree:art::tree(r,e.x,e.y,128+e.id,v.season,faded(e,s)?4:8,false,e.growth,farm::WildTrees[e.id].variant);break;
      case Kind::Ore:art::mine_ore(r,e.x,e.y,farm::ore_item(s,e.id));break;
      case Kind::Monster:{auto &m=s.mobs[e.id];art::mine_monster(r,e.x,e.y,farm::mob_type(s,e.id),v.animation,m.dir,m.windup>0);
        if(m.hp<farm::mob_max_hp(s,e.id)){r.rect(e.x-9,e.y-32,18,3,art::Ink);r.rect(e.x-8,e.y-31,std::max(1,16*m.hp/farm::mob_max_hp(s,e.id)),1,art::Gold);}break;}
      case Kind::CaveCrop:art::food(r,e.x,e.y,farm::cave_forage_item(s,e.id));break;
      case Kind::MineEntrance:art::mine_entrance(r,e.x,e.y);break;
      case Kind::MineLadder:if(e.id&&s.mine_depth==30)art::grounded(r,sprites::chest,e.x,e.y);else art::mine_ladder(r,e.x,e.y,e.id,e.id?farm::ladder_ready(s):true);break;
      case Kind::MineTorch:art::mine_torch(r,e.x,e.y,v.animation);break;
      case Kind::VillageBoard:art::village_board(r,e.x,e.y);break;
      case Kind::Bench:art::village_bench(r,e.x,e.y);break;
      case Kind::Planter:art::village_planter(r,e.x,e.y,farm::indoors(s.region)?0:v.season,farm::indoors(s.region)||s.friend_chapter[0]>=2);break;
      case Kind::Animal:art::animal(r,e.x,e.y,s.animals[e.id],e.growth&128?3:1,e.growth&127);break;
      case Kind::RanchShed:art::ranch_shed(r,e.x,e.y,s.trough,s.ranch_chapter>=5,farm::ranch_lit(s));break;
      case Kind::WorldNpc: {
        auto p = placement(e);
        art::gait::draw(r,e.x,e.y,e.id/4,e.growth,false,&art::villager_rig(e.id%4,e.id/4));
        break;
      }
      case Kind::Counter:
        art::world_counter(r,e.x,e.y);
        break;
      case Kind::Cabinet:art::furniture(r,*furniture_image(e),e.x,e.y);break;
      case Kind::KitchenCounter:
        art::furniture(r,furniture_sprites::CounterBay,e.x,e.y);
        r.rect(e.x-14,e.y-27,15,3,art::Cream);r.line(e.x-12,e.y-26,e.x-4,e.y-26,art::Gold);
        art::food(r,e.x+10,e.y-26,22);break;
      case Kind::Fireplace:art::world_fireplace(r,e.x,e.y,v.animation);break;
      case Kind::Chair:art::furniture(r,*furniture_image(e),e.x,e.y,s.region>=uint8_t(farm::Region::SeedHome)?e.id:0);break;
      case Kind::Shelf:art::world_shelf(r,e.x,e.y);break;
      case Kind::Signpost:art::world_signpost(r,e.x,e.y,farm::portal_heading(farm::Portals[e.id]));break;
      case Kind::Television: case Kind::Calendar:
        art::world_appliance(r,e.x,e.y,e.kind==Kind::Television);break;
      case Kind::Bed:
        art::world_bed(r, e.x, e.y,e.id);
        break;
      case Kind::Stove:
        art::world_stove(r,e.x,e.y);
        break;
      case Kind::Table:
        art::world_table(r, e.x, e.y,s.region>=uint8_t(farm::Region::SeedHome)?e.id+1:0);
        break;
      case Kind::Forage: {
        auto p = placement(e);
        if (p.sprite) art::sprite(r, *p.sprite, p.x, p.y);
        else art::food(r,e.x,e.y,farm::Foraging[e.id].item);
        break;
      }
      case Kind::Tree:
        art::tree(r, e.x, e.y, e.id, v.season, faded(e, s) ? 4 : 8, false,
                  e.growth,e.id==255?3:-1);
        break;
      case Kind::Rock:
      case Kind::Grass:
      case Kind::Well:
      case Kind::Chest:
      case Kind::Board: {
        auto p = placement(e);
        art::grounded(r, *p.sprite, p.x, p.y);
        break;
      }
      case Kind::House:
        art::village_house(r,e.x,e.y,e.id);
        if(e.id<=2&&s.region==uint8_t(farm::Region::Town)&&
          !farm::building_open(s,e.id==1?farm::Region::SeedShop:farm::Region::Lodge)) {
          r.line(e.x+36,e.y+44,e.x+40,e.y+49,art::Ink);
          r.line(e.x+45,e.y+44,e.x+42,e.y+49,art::Ink);
          r.rect(e.x+33,e.y+49,17,8,art::Ink);
          r.rect(e.x+34,e.y+50,15,6,farm_palette::WoodDark);
          r.rect(e.x+35,e.y+50,13,1,art::Gold);
          r.rect(e.x+39,e.y+52,5,3,art::Cream);
        }
        break;
      case Kind::Fence:
        art::fence(r, e.x, e.y, 48);
        break;
      case Kind::Crop: {
        auto &p = s.plots[e.id];
        auto &c = farm::Crops[p.crop - 1];
        art::crop(r, e.x, e.y, p.crop - 1,
                  p.growth == 0        ? 0
                  : p.growth >= c.days ? 3
                                       : p.growth * 3 / c.days,
                  p.growth >= c.days, p.wet);
        break;
      }
      case Kind::Sprinkler:
        art::grounded(r, sprites::sprinkler, e.x, e.y);
        break;
      case Kind::Villager:
        art::person(r, e.x, e.y, 2, 0, true, 0, 0, false);
        break;
      case Kind::Chicken:
        art::chicken(r, e.x, e.y, v.animation / 2 + e.id);
        break;
      case Kind::Player:
        if(farm::in_mine(s)&&s.invulnerable&&(s.invulnerable/100)%2)break;
        art::person(r, e.x, e.y, v.direction, v.frame, false, v.swing, v.tool,
                    false, v.held_item, v.grade, v.seed, v.fishing,v.tool_target,s.appearance);
        break;
      case Kind::RemotePlayer: {
        auto&p=v.farmers[e.id];
        art::person(r,e.x,e.y,p.direction,p.frame,false,p.swing,p.tool,
                    false,p.item,p.grade,p.seed,false,p.target,p.appearance);
        uint16_t colors[]={farm_palette::Brass,farm_palette::Sage,art::rgb(94,151,174),art::rgb(181,111,108)};
        int top=e.y-(farm::carrying_item(p.item)?59:34);
        r.rect(e.x-4,top,9,4,art::Ink);r.rect(e.x-3,top+1,7,2,colors[(p.number-1)&3]);
        break;
      }
      }
      if(v.hit_ms&&(e.kind==Kind::WildTree||e.kind==Kind::Ore)&&v.hit_id==(e.kind==Kind::WildTree?32:96)+e.id) {
        int hp=e.kind==Kind::WildTree?farm::wild_hp(s,e.id):farm::ore_hp(s,e.id),damage=e.kind==Kind::WildTree?s.wild_damage[e.id]:s.ore_damage[e.id];
        if(damage&&hp){r.rect(e.x-7,e.y-24,14,3,art::Ink);r.rect(e.x-6,e.y-23,std::max(1,12*(hp-damage)/hp),1,art::Gold);}
      }
      if ((e.kind == Kind::Tree || e.kind == Kind::Rock ||
           e.kind == Kind::Grass) &&
          e.interactive && e.id < 24 && v.hit_ms && v.hit_id == e.id &&
          s.damage[e.id]) {
        int hp = farm::obstacle_hp(e.id);
        if (e.kind == Kind::Tree && e.growth < 2)
          hp = e.growth == 0 ? 1 : 3;
        int yy = e.y - (e.kind == Kind::Tree  ? 15
                        : placement(e).sprite ? placement(e).sprite->h + 4
                                              : 14);
        r.rect(e.x - 6, yy, 12, 3, art::Ink);
        r.rect(e.x - 5, yy + 1, std::max(1, 10 * (hp - s.damage[e.id]) / hp), 1,
               farm_palette::Brass);
      }
    }
  }
  bool pick(int x, int y, const farm::State &s, int &tx, int &ty,int *animal_id=nullptr,int *npc_id=nullptr) const {
    if(animal_id)*animal_id=-1;if(npc_id)*npc_id=-1;
    if(const auto*p=farm::walls::front_exit(s.region);p&&farm::walls::FrontDoor.contains(x,y)){tx=p->x;ty=p->y;return true;}
    for (unsigned at = count; at > 0; --at) {
      auto &e = entries[at - 1];
      if (!e.interactive)
        continue;
      auto p = placement(e);
      if(e.kind==Kind::VillageBoard&&bounds(e).intersects({x,y,1,1})){tx=18;ty=10;return true;}
      if(e.kind==Kind::MineEntrance&&Rect{e.x-10,e.y-16,21,20}.intersects({x,y,1,1})){tx=22;ty=3;return true;}
      if(e.kind==Kind::WildTree){auto p=placement(e);if(art::hit(*p.sprite,x-p.x+p.sprite->anchor_x,y-p.y+p.sprite->anchor_y)){tx=farm::WildTrees[e.id].x;ty=farm::WildTrees[e.id].y;return true;}continue;}
      if((e.kind==Kind::Ore||e.kind==Kind::Monster||e.kind==Kind::CaveCrop||e.kind==Kind::MineLadder)&&bounds(e).intersects({x,y,1,1})){tx=e.x/16;ty=e.y/16;return true;}
      if(e.kind==Kind::Animal) {
        auto &sprite=art::animal_sprite(s.animals[e.id],e.growth&128?3:1,e.growth&127);
        if(art::animal_hit(sprite,x-e.x+16,y-e.y+27)){tx=e.x/16;ty=e.y/16;if(animal_id)*animal_id=e.id;return true;}
        continue;
      }
      if(e.kind==Kind::RanchShed&&bounds(e).intersects({x,y,1,1})){tx=e.x/16;ty=e.y/16;return true;}
      if(e.kind==Kind::Signpost && bounds(e).intersects({x,y,1,1})) {
        tx=farm::Portals[e.id].x;ty=farm::Portals[e.id].y;return true;
      }
      if (e.kind == Kind::Bed && bounds(e).intersects({x, y, 1, 1})) {
        tx = 10;
        ty = 9;
        return true;
      }
      if ((e.kind == Kind::Stove || (e.kind == Kind::Forage && !p.sprite)) &&
          bounds(e).intersects({x,y,1,1})) {
        tx = e.kind == Kind::Stove ? 21 : farm::Foraging[e.id].x;
        ty = e.kind == Kind::Stove ? 10 : farm::Foraging[e.id].y;
        return true;
      }
      if((e.kind==Kind::Television||e.kind==Kind::Calendar||e.kind==Kind::Table||e.kind==Kind::Counter)&&bounds(e).intersects({x,y,1,1})) {tx=e.kind==Kind::Counter?std::clamp(x/16,12,19):e.x/16;ty=e.y/16;return true;}
      if (!p.sprite)
        continue;
      if(e.kind==Kind::House) {
        // The entire visible doorway frame is an entrance, including the
        // anti-aliased/transparent trim pixels around the sprite's silhouette.
        for(int dx:{40,72}){
          if(dx==72&&e.id!=1)continue;
          if(Rect{e.x+dx-13,e.y+26,27,44}.intersects({x,y,1,1})){
            tx=(e.x+dx)/16;ty=e.y/16+4;return true;
          }
        }
        continue;
      }
      if (e.kind == Kind::Tree && faded(e, s) &&
          y < p.y - std::max(3, int(p.sprite->h) / 3))
        continue;
      if (!art::hit(*p.sprite, x - p.x + p.sprite->anchor_x,
                    y - p.y + p.sprite->anchor_y))
        continue;
      if (e.kind == Kind::WorldNpc) {
        if(npc_id)*npc_id=e.id%4;
        tx = e.x / 16;
        ty = e.y / 16;
      } else if (e.kind == Kind::Forage) {
        tx = farm::Foraging[e.id].x;
        ty = farm::Foraging[e.id].y;
      } else if (e.kind == Kind::House) {
        auto door=farm::Buildings[e.id].door();tx=door.x;ty=door.y;
        if(e.id==1&&x>=e.x+64){tx=9;ty=7;}
      } else if (e.kind == Kind::Villager || e.kind == Kind::Board) {
        tx = s.region == uint8_t(farm::Region::Town) ? 18 : 21;
        ty = s.region == uint8_t(farm::Region::Town) ? 10 : 8;
      } else if (e.kind == Kind::Well) {
        tx = s.region == uint8_t(farm::Region::Town) ? 16 : farm::WellX;
        ty = s.region == uint8_t(farm::Region::Town) ? 10 : farm::WellY;
      } else if (e.kind == Kind::Chest) {
        tx = farm::ShippingX;
        ty = farm::ShippingY;
      } else {
        auto &o = farm::Obstacles[e.id];
        tx = o.x;
        ty = o.y;
      }
      return true;
    }
    return false;
  }

private:
  uint16_t next_order = 0;
};
} // namespace farm_scene
