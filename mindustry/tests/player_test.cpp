// SPDX-License-Identifier: GPL-3.0-only
#include "../core.hpp"
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <vector>
using namespace md;
int checks = 0;
#define CHECK(x)                                                               \
  do {                                                                         \
    ++checks;                                                                  \
    if (!(x)) {                                                                \
      std::fprintf(stderr, "FAIL %d: %s\n", __LINE__, #x);                     \
      std::exit(1);                                                            \
    }                                                                          \
  } while (0)
State world() {
  State s;
  s.generate(154, 0, true);
  for (int i = 0; i < MaxBuildings; ++i)
    if (s.buildings[i].type > Core)
      s.remove(i);
  for (auto &n : s.stock)
    n = 1000;
  return s;
}
void ticks(State &s, int n) {
  for (int i = 0; i < n; ++i)
    s.step();
}
int main(int argc,char **argv) {
  if(argc>1){
    State fixture;fixture.generate(154,0,false);
    int xx=-1,yy=-1;
    for(int y=2;y<44 && xx<0;++y)for(int x=2;x<44;++x)
      if(fixture.canMine(x,y) && fixture.tiles[y*W+x].ore==Copper && std::hypot(x-24.f,y-36.f)>31){xx=x;yy=y;break;}
    if(xx<0)return 2;
    fixture.playerX=fixture.targetX=xx+.5f;fixture.playerY=fixture.targetY=yy+.5f;
    std::array<uint8_t,3900> data{};auto n=fixture.save(data.data(),data.size());if(!n)return 3;
    auto *file=std::fopen(argv[1],"wb");if(!file)return 4;std::fwrite(data.data(),1,n,file);std::fclose(file);
    std::printf("%d %d\n",xx,yy);return 0;
  }
  auto s = world();
  CHECK(!s.mine(17, 27));
  CHECK(!s.mine(17, 22)); // Coal/Ti exceed Alpha tier.
  CHECK(s.mine(19, 33));
  auto initial = s.stock[Copper];
  for (int i = 0; i < 1000; ++i)
    CHECK(s.mine(19, 33));
  CHECK(s.stock[Copper] == initial);
  ticks(s, 3);
  CHECK(s.stock[Copper] == initial);
  s.step();
  CHECK(s.stock[Copper] == initial + 1);
  ticks(s, 16);
  CHECK(s.stock[Copper] == initial + 6 && s.carriedCount == 0);
  s.movePlayer(24.5f, 38.5f);
  CHECK(s.mineX < 0);
  ticks(s, 20);
  CHECK(s.stock[Copper] == initial + 6);
  auto sand = world();
  sand.playerX = 28.5f;
  sand.playerY = 30.5f;
  CHECK(sand.mine(28, 30));
  initial = sand.stock[Sand];
  ticks(sand, 40);
  CHECK(sand.stock[Sand] == initial + 15);
  auto carry = world();
  carry.tiles[4 * W + 4] = {Stone, Copper};
  carry.playerX = carry.targetX = 4.5f;
  carry.playerY = carry.targetY = 4.5f;
  CHECK(carry.mine(4, 4));
  ticks(carry, 100);
  CHECK(carry.carriedCount == 30 && carry.carriedItem == Copper &&
        carry.stock[Copper] == 1000);
  ticks(carry, 4);
  CHECK(carry.mineX < 0 && carry.carriedCount == 30);
  carry.tiles[4 * W + 5] = {Stone, Lead};
  CHECK(carry.mine(5, 4));
  ticks(carry, 4);
  CHECK(carry.mineX < 0 && carry.carriedItem == Copper &&
        carry.carriedCount == 30);
  carry.movePlayer(24.5f, 38.5f);
  carry.playerX = 24.5f;
  carry.playerY = 38.5f;
  carry.stock[Copper] = 3995;
  carry.supply(carry.core());
  CHECK(carry.stock[Copper] == 4000 && carry.carriedCount == 25);
  carry.stock[Copper] = 3900;
  carry.supply(carry.core());
  CHECK(carry.carriedCount == 0 && carry.carriedItem == None &&
        carry.stock[Copper] == 3925);
  int g = carry.build(Generator, 26, 31, 0, true);
  CHECK(g >= 0);
  carry.carriedItem = Coal;
  carry.carriedCount = 10;
  initial = carry.stock[Coal];
  carry.supply(g);
  CHECK(carry.buildings[g].inv[Coal] == 10 && carry.stock[Coal] == initial &&
        !carry.carriedCount);
  auto pause = world();
  pause.sandbox=false;pause.researched=(1u<<Blocks)-1;
  CHECK(pause.mine(19, 33));
  pause.step();
  auto progress = pause.mineProgress;
  CHECK(pause.queueBuild(Press, 27, 31) >= 0);
  pause.step();
  CHECK(pause.mineProgress == progress && !pause.mining);
  auto pending = world();
  CHECK(pending.mine(4, 4) == pending.canMine(4, 4));
  pending.tiles[4 * W + 4] = {Stone, Copper};
  CHECK(pending.mine(4, 4));
  ticks(pending, 10);
  CHECK(pending.mineProgress == 0 && !pending.mining);
  auto saved = world();
  CHECK(saved.mine(19, 33));
  ticks(saved, 3);
  saved.carriedItem = Lead;
  saved.carriedCount = 12;
  std::array<uint8_t, 3900> bytes{}, again{};
  auto n = saved.save(bytes.data(), bytes.size());
  CHECK(n > 0 && bytes[3] == '8');
  State restored;
  CHECK(restored.load(bytes.data(), n));
  CHECK(restored.carriedCount == 12 &&
        restored.mineProgress == saved.mineProgress);
  CHECK(restored.save(again.data(), again.size()) == n &&
        !std::memcmp(bytes.data(), again.data(), n));
  saved.step();
  restored.step();
  CHECK(saved.stock[Copper] == restored.stock[Copper] &&
        saved.mineProgress == restored.mineProgress);
  std::ifstream old("local/pxa-apps/mindustry/tests/fixtures/v0.7.mnd",
                    std::ios::binary);
  std::vector<uint8_t> v6((std::istreambuf_iterator<char>(old)), {});
  CHECK(!v6.empty() && restored.load(v6.data(), v6.size()));
  CHECK(!restored.carriedCount && restored.carriedItem == None &&
        restored.mineX == -1);
  // Shared flow fields only rebuild for a changed goal or walkability.
  auto nav = world();
  nav.command = Command::Rally;
  nav.units[0] = {24, 30, 150, 0};
  nav.rallyX = 25.5f;
  nav.rallyY = 30.5f;
  ticks(nav, 100);
  CHECK(nav.unitNavRebuilds == 1 && nav.flowRebuilds == 0);
  nav.rallyX = 26.5f;
  nav.step();
  CHECK(nav.unitNavRebuilds == 2);
  CHECK(nav.build(Wall, 23, 30, 0, true) >= 0);
  nav.step();
  CHECK(nav.unitNavRebuilds == 3);
  auto air = world();
  air.units[0] = {24, 30, 70, 1};
  ticks(air, 100);
  CHECK(air.unitNavRebuilds == 0 && air.flowRebuilds == 0);
  auto enemy = world();
  CHECK(enemy.startWave());
  ticks(enemy, 100);
  CHECK(enemy.flowRebuilds == 1);
  CHECK(enemy.build(Wall, 22, 30, 0, true) >= 0);
  enemy.step();
  CHECK(enemy.flowRebuilds == 1);
  std::printf("%d player/navigation checks passed: mining timing/tier, carry, "
              "transfers, construction priority, saves and cached paths\n",
              checks);
}
