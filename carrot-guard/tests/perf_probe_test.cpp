// SPDX-License-Identifier: MIT
#define CARROT_PERF_PROBE 1
#define CARROT_STRESS_PROBE 1
#define CARROT_LIVE_PROBE 1
#include "../main.cpp"
#include <cassert>
extern "C" int32_t pxa_call(uint32_t,uint64_t,const uint8_t*,uint32_t){return 0;}
extern "C" int32_t pxa_log_write(uint32_t,const char*){return 0;}
extern "C" int32_t pxa_io(uint64_t,uint32_t,uint8_t*,uint32_t){return 0;}
int main(){
  screen.width=480;screen.height=854;layout();
  updatePerfProbe(1000);
  int towers=0;for(const auto& t:game.towers)towers+=t.type!=0;
  assert(towers==24&&game.countEnemies()==36);
  bool moved=false,fired=false;auto original=game.enemies[0].p;
  for(unsigned tick=1;tick<=500;++tick){
    updatePerfProbe(1000+tick*20000);game.step(.02f);
    for(const auto& shot:game.shots)fired|=shot.active;
    moved|=distanceSquared(original,game.enemies[0].p)>1;
    assert(game.countEnemies()<=MaxEnemies);
  }
  assert(moved&&fired&&game.kills>0);
  updatePerfProbe(20001000);
  towers=0;for(const auto& t:game.towers)towers+=t.type!=0;
  assert(towers==24&&game.countEnemies()==36);
  std::puts("Live pressure fixture: 24 visible towers, 36 enemies, real movement, projectiles and kills passed");
}
