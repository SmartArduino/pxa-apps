// SPDX-License-Identifier: MIT
// Native QA utility: creates reproducible saves, or inspects a simulator save.
#include "../core.hpp"
#include <cstdio>
#include <cstring>
#include <vector>
using namespace starsoil;
static State s;
int main(int argc, char **argv) {
  if (argc != 3) {
    std::fprintf(stderr,
                 "usage: save_fixture "
                 "inspect|wardrobe|plain|cloth|copper|iron|robe|bow|wand|"
                 "forest|desert|snow|cave|warden|jungle|night|joins FILE\n");
    return 2;
  }
  std::vector<uint8_t> bytes(SaveBytes);
  if (std::strcmp(argv[1], "inspect") == 0) {
    FILE *f = std::fopen(argv[2], "rb");
    if (!f)
      return 2;
    size_t n = std::fread(bytes.data(), 1, bytes.size(), f);
    std::fclose(f);
    if (!decode(s, bytes.data(), n))
      return 3;
    std::printf("{\"seed\":%u,\"x\":%.2f,\"y\":%.2f,\"mined\":%u,\"crafted\":%"
                "u,\"defense\":%d,\"selected\":%u,\"wood\":%d,\"head\":%u,"
                "\"shirt\":%u,\"cape\":%u,\"bytes\":%zu}\n",
                s.seed, s.player.x, s.player.y, s.mined, s.crafted, defense(s),
                s.selected, count(s, Wood), s.equip[0], s.equip[3], s.equip[4],
                n);
    return 0;
  }
  new_game(s, 1234567);
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
