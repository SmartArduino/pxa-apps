// SPDX-License-Identifier: MIT
// Native QA utility: creates reproducible saves, or inspects a simulator save.
#include "../core.hpp"
#include "../save_codec.hpp"
#include <cstdio>
#include <cstring>
#include <vector>
using namespace starsoil;
static State s;
int main(int argc, char **argv) {
  if (argc != 3) {
    std::fprintf(stderr,
                 "usage: save_fixture "
                 "inspect|wardrobe|plain|cloth|copper|iron|robe|pick|axe|bow|wand|"
                 "forest|desert|snow|cave|warden|jungle|night|joins|doors FILE\n");
    return 2;
  }
  std::vector<uint8_t> bytes(starsoil_save::buffer_capacity(SaveBytes));
  if (std::strcmp(argv[1], "inspect") == 0) {
    FILE *f = std::fopen(argv[2], "rb");
    if (!f)
      return 2;
    size_t n = std::fread(bytes.data(), 1, bytes.size(), f);
    std::fclose(f);
    if (!starsoil_save::unpack(bytes.data(), n, bytes.size(), SaveBytes) || !decode(s, bytes.data(), n))
      return 3;
    std::printf("{\"seed\":%u,\"x\":%.2f,\"y\":%.2f,\"mined\":%u,\"crafted\":%"
                "u,\"defense\":%d,\"selected\":%u,\"wood\":%d,\"head\":%u,"
                "\"shirt\":%u,\"cape\":%u,\"bytes\":%zu,\"treasure_chests\":%d,\"star_cores\":%d}\n",
                s.seed, s.player.x, s.player.y, s.mined, s.crafted, defense(s),
                s.selected, count(s, Wood), s.equip[0], s.equip[3], s.equip[4],
                n, count(s, TreasureChest), count(s, StarCore));
    return 0;
  }
  new_game(s, 1234567, std::strcmp(argv[1], "performance-huge") == 0 ? WorldSize::Huge : WorldSize::Medium);
  auto save = [&]() {
    size_t n = encode(s, bytes.data(), bytes.size());
    FILE *f = std::fopen(argv[2], "wb"); if (!f) return 2;
    bool ok = std::fwrite(bytes.data(), 1, n, f) == n;
    std::fclose(f); return ok ? 0 : 4;
  };
  if (std::strcmp(argv[1], "performance-huge") == 0 ||
      std::strcmp(argv[1], "performance-medium") == 0) return save();
  if (std::strcmp(argv[1], "polish") == 0) {
    for (auto &slot : s.bag) slot = {};
    s.bag[0]={WoodPick,1};s.bag[1]={WoodAxe,1};s.bag[2]={WoodSword,1};
    s.bag[3]={Torches,24};s.bag[8]={Wood,990};s.bag[18]={Wood,20};
    s.bag[21]={CopperHelmet,1};s.bag[22]={HunterBow,1};s.bag[23]={Wood,7};
    s.bag[25]={IronPick,1};s.bag[26]={Boards,10};s.bag[30]={Torches,20};
    s.chest[0]={Wood,980};s.chest[5]={Rock,10};s.chest[10]={Rock,20};
    s.player.x=48*16+8;s.player.y=31*16;s.clock=100000;s.selected=23;
    set_tile(s,50,30,StorageChest);
    return save();
  }
  if (std::strcmp(argv[1], "smart-cursor") == 0 || std::strcmp(argv[1], "smart-bridge") == 0) {
    bool bridge=std::strcmp(argv[1],"smart-bridge")==0;
    std::fill(std::begin(s.tiles),std::end(s.tiles),uint8_t(Air));
    std::fill(std::begin(s.walls),std::end(s.walls),uint8_t(0));
    std::fill(std::begin(s.heights),std::end(s.heights),uint8_t(60));
    for(int x=1;x<s.width-1;++x) {
      if(!bridge || x<=100)set_tile(s,x,60,Stone);
      s.walls[58*WorldW+x]=Plank;
    }
    for(auto &slot:s.bag)slot={};
    s.bag[0]={uint8_t(bridge?Boards:IronPick),uint16_t(bridge?12:1)};
    if(!bridge)for(int x=101;x<=103;++x)set_tile(s,x,57,Stone);
    s.player.x=100*16+8;s.player.y=60*16;s.clock=100000;s.selected=0;
    return save();
  }
  if (std::strcmp(argv[1], "aim-jump") == 0) {
    std::fill(std::begin(s.tiles),std::end(s.tiles),uint8_t(Air));
    std::fill(std::begin(s.walls),std::end(s.walls),uint8_t(0));
    std::fill(std::begin(s.heights),std::end(s.heights),uint8_t(60));
    for(int x=1;x<s.width-1;++x) {
      set_tile(s,x,60,Stone);
      for(int y=54;y<60;++y)s.walls[y*WorldW+x]=Plank;
    }
    for(auto &slot:s.bag)slot={};
    s.bag[0]={HunterBow,1};s.bag[1]={IronPick,1};add(s,Arrows,999);
    s.equip[0]=CrystalHelmet;s.equip[1]=CrystalMail;s.equip[2]=CrystalGreaves;
    s.equip[3]=RedTunic;s.equip[4]=EmberCape;s.equip[5]=CloudBoots;
    s.player.x=100*16+8;s.player.y=60*16;s.clock=100000;s.selected=0;
    return save();
  }
  if (std::strcmp(argv[1], "exploration") == 0) {
    std::fill(std::begin(s.tiles),std::end(s.tiles),uint8_t(Air));
    std::fill(std::begin(s.walls),std::end(s.walls),uint8_t(0));
    std::fill(std::begin(s.heights),std::end(s.heights),uint8_t(40));
    for(int x=1;x<s.width-1;++x)set_tile(s,x,40,Stone);
    for(auto &slot:s.bag)slot={};
    s.bag[0]={WoodPick,1};s.bag[1]={WoodAxe,1};s.bag[2]={WoodSword,1};
    s.bag[3]={Boards,10};s.bag[20]={WoodSword,1};
    s.player.x=100*16+8;s.player.y=40*16;s.clock=100000;s.selected=0;
    s.home_x=100;s.home_y=40;
    set_tile(s,96,39,Anvil);set_tile(s,97,39,Furnace);set_tile(s,98,39,Bench);
    set_tile(s,99,39,Bed);set_tile(s,101,39,Chest);set_tile(s,104,39,StorageChest);
    set_tile(s,103,38,DoorTop);set_tile(s,103,39,Door);
    return save();
  }
  if (std::strcmp(argv[1], "inventory") == 0) {
    for (auto &slot : s.bag) slot = {};
    const Slot slots[] = {{WoodPick,1},{WoodAxe,1},{WoodSword,1},{Torches,10},
      {Boards,12},{HealPotion,3},{Doors,2},{Wood,10},{Wood,990},{Wood,20},
      {CopperHelmet,1},{CrystalPick,1},{WoodSword,1},{Boards,20},{HunterBow,1},
      {StarWand,1},{HealPotion,3},{Arrows,60},{IronAxe,1}};
    std::copy(std::begin(slots),std::end(slots),s.bag);
    s.player.x=48*16+8; s.player.y=31*16; s.clock=100000; s.selected=0;
    set_tile(s,50,30,Stone);
    return save();
  }
  if (std::strncmp(argv[1], "background-", 11) == 0) {
    int y = std::strcmp(argv[1],"background-shallow")==0 ? 35 :
            std::strcmp(argv[1],"background-cave")==0 ? 36 :
            std::strcmp(argv[1],"background-crystal")==0 ? 68 : 32;
    for (auto &h : s.heights) h=32;
    for (int yy=10;yy<92;++yy) for(int xx=90;xx<135;++xx) {
      set_tile(s,xx,yy,Air);s.walls[yy*WorldW+xx]=0;
    }
    for(int xx=90;xx<135;++xx)set_tile(s,xx,y,Platform);
    s.player.x=110*16+8;s.player.y=y*16;s.clock=100000;s.hp=100;
    return save();
  }
  if (std::strcmp(argv[1], "doors") == 0 || std::strcmp(argv[1], "doors-legacy-gap") == 0) {
    s.player.x = 50 * 16.0f; s.player.y = 31 * 16.0f;
    s.clock = 100000; s.hp = 100; s.selected = 0;
    if (std::strcmp(argv[1], "doors-legacy-gap") == 0) {
      set_tile(s, 53, 29, Door); set_tile(s, 53, 30, Air); set_tile(s, 53, 31, Air);
    }
    size_t n = encode(s, bytes.data(), bytes.size());
    FILE *f = std::fopen(argv[2], "wb"); if (!f) return 2;
    bool ok = std::fwrite(bytes.data(), 1, n, f) == n;
    std::fclose(f); return ok ? 0 : 4;
  }
  int x = 48, y = 31;
  bool motion = std::strcmp(argv[1], "motion") == 0 ||
                std::strcmp(argv[1], "motion-tool") == 0;
  if (motion)
    x = 90;
  if (std::strcmp(argv[1], "forest") == 0)
    x = 61;
  if (std::strcmp(argv[1], "desert") == 0)
    x = 190;
  if (std::strcmp(argv[1], "snow") == 0)
    x = 254;
  if (std::strcmp(argv[1], "jungle") == 0)
    x = 335;
  if (std::strcmp(argv[1], "joins") == 0)
    x = 110;
  if (std::strcmp(argv[1], "cave") == 0 ||
      std::strcmp(argv[1], "warden") == 0) {
    x = 194;
    y = 92;
  } else
    y = s.heights[x];
  s.player.x = x * 16 + 8;
  s.player.y = y * 16;
  if (std::strcmp(argv[1], "night") == 0)
    s.clock = 270000;
  if (motion) {
    for (int xx = x - 16; xx <= x + 16; ++xx) {
      s.heights[xx] = uint8_t(y);
      for (int yy = y - 9; yy < y + 4; ++yy) {
        set_tile(s, xx, yy, yy < y ? Air : yy == y ? Grass : Dirt);
        s.walls[yy * WorldW + xx] = Air;
      }
    }
  }
  if (std::strcmp(argv[1], "joins") == 0) {
    for (int xx = x - 10; xx <= x + 10; ++xx) {
      s.heights[xx] = uint8_t(y);
      for (int yy = y - 9; yy < y + 5; ++yy) {
        set_tile(s, xx, yy, yy < y ? Air : yy == y ? Grass : Dirt);
        s.walls[yy * WorldW + xx] = Air;
      }
    }
    for (int xx = x - 8; xx <= x - 4; ++xx)
      for (int yy = y - 3; yy < y; ++yy)
        set_tile(s, xx, yy, Stone);
    set_tile(s, x - 8, y - 3, Air);
    set_tile(s, x - 5, y - 2, Air);
    set_tile(s, x - 5, y - 3, Air);
    set_tile(s, x - 7, y - 1, CopperOre);
    set_tile(s, x - 6, y - 1, CopperOre);
    for (int xx = x + 4; xx <= x + 7; ++xx)
      for (int yy = y - 4; yy <= y - 2; ++yy)
        set_tile(s, xx, yy, yy == y - 4 ? Grass : Dirt);
    set_tile(s, x + 4, y - 2, Air);
    set_tile(s, x + 5, y - 2, Air);
    set_tile(s, x + 7, y - 3, Air);
    set_tile(s, x + 1, y - 5, Snow);
    set_tile(s, x + 2, y - 5, Ice);
  }
  for (int yy = y - 3; yy < y; ++yy)
    for (int xx = x - 1; xx <= x + 1; ++xx)
      if (solid(tile(s, xx, yy)) || tile(s, xx, yy) == Door)
        set_tile(s, xx, yy, Air);
  int gear[] = {CrystalHelmet, CrystalMail, CrystalGreaves, RedTunic,
                EmberCape,     CloudBoots,  GlowCharm};
  if (std::strcmp(argv[1], "plain") == 0 ||
      std::strcmp(argv[1], "reference") == 0 || motion) {
    gear[0] = gear[1] = gear[2] = gear[4] = gear[6] = 0;
    gear[5] = SwiftBoots;
    gear[3] = (std::strcmp(argv[1], "reference") == 0 || motion) ? BlueTunic
                                                                 : RedTunic;
  } else if (std::strcmp(argv[1], "copper") == 0) {
    gear[0] = CopperHelmet;
    gear[1] = CopperMail;
    gear[2] = CopperGreaves;
    gear[5] = SwiftBoots;
  } else if (std::strcmp(argv[1], "iron") == 0) {
    gear[0] = IronHelmet;
    gear[1] = IronMail;
    gear[2] = IronGreaves;
    gear[3] = BlueTunic;
    gear[4] = TealCape;
    gear[5] = SwiftBoots;
    gear[6] = Magnet;
  } else if (std::strcmp(argv[1], "cloth") == 0 ||
             std::strcmp(argv[1], "robe") == 0) {
    gear[0] = StrawHat;
    gear[1] = gear[2] = 0;
    gear[3] = std::strcmp(argv[1], "robe") == 0 ? SageRobe : BlueTunic;
    gear[4] = TealCape;
  }
  std::memset(s.equip, 0, sizeof(s.equip));
  for (int id : gear) {
    if (!id)
      continue;
    add(s, id, 1);
    for (int i = 0; i < BagSlots; ++i)
      if (s.bag[i].item == id) {
        equip(s, i);
        break;
      }
  }
  s.bag[0] = {CrystalPick, 1};
  s.bag[1] = {HunterBow, 1};
  s.bag[2] = {CrystalSword, 1};
  s.bag[4] = {StarWand, 1};
  s.bag[5] = {IronPick, 1};
  s.bag[6] = {HealPotion, 12};
  if (std::strcmp(argv[1], "pick") == 0) s.selected = 0;
  if (std::strcmp(argv[1], "axe") == 0) {
    s.bag[0] = {IronAxe, 1}; s.selected = 0;
  }
  if (std::strcmp(argv[1], "bow") == 0)
    s.selected = 1;
  if (std::strcmp(argv[1], "wand") == 0)
    s.selected = 4;
  add(s, Arrows, 60);
  for (auto id : {Copper, Iron, Gold, Coal, Crystal, Ruby, Silk, StarDust,
                  CopperBar, IronBar, GoldBar})
    add(s, id, 60);
  // Place real crafting stations near the fixture's player.
  set_tile(s, x + 2, y - 1, Bench);
  set_tile(s, x + 3, y - 1, Furnace);
  set_tile(s, x + 4, y - 1, Anvil);
  set_tile(s, x - 3, y - 2, Torch);
  if (std::strcmp(argv[1], "warden") == 0)
    add(s, StarCore, 1);
  if (std::strcmp(argv[1], "reference") == 0 ||
      std::strcmp(argv[1], "motion") == 0) {
    s.bag[7] = {};
    s.selected = 7;
  }
  size_t n = encode(s, bytes.data(), bytes.size());
  FILE *f = std::fopen(argv[2], "wb");
  if (!f)
    return 2;
  bool ok = std::fwrite(bytes.data(), 1, n, f) == n;
  std::fclose(f);
  return ok ? 0 : 4;
}
