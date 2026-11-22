// SPDX-License-Identifier: MIT
// Included only by CARROT_PERF_PROBE. No save writes. CARROT_LIVE_PROBE
// maintains the workload while running real movement, targeting and projectiles.
// CARROT_GALLERY rotates the six environments for actual simulator captures.
void updatePerfProbe(uint64_t now) {
  static uint64_t epoch = 0;
  static int phase = -1;
  if (!epoch)
    epoch = now;
  int next = std::min(1, int((now - epoch) / 18000000));
#ifdef CARROT_GALLERY
  next=int((now-epoch)/8000000)%6;
#endif
  if (next != phase) {
    phase = next;
#ifdef CARROT_STRESS_PROBE
    game.unlock(Levels);
    game.reset(17);
#ifdef CARROT_GALLERY
    game.reset(next*3+2);
#endif
    game.coins = 100000;
    int placed = 0;
    for (int row = 1; row < GridRows - 1 && placed < 24; ++row)
      for (int col = 9; col < 23 && placed < 24; ++col) {
        int slot = row * GridColumns + col;
        if (game.land(slot) && !game.towers[slot].obstacle &&
            game.build(slot, placed % TowerTypes + 1)) {
          game.towers[slot].level=1+placed/6%3;
          ++placed;
        }
      }
    game.coins=320;
    int kinds[] = {0,1,2,4,5,7,9};
    int boss=14;
#ifdef CARROT_GALLERY
    int bosses[]={3,10,11,12,13,14};boss=bosses[next];
#endif
    for (int i = 0; i < 36; ++i)
      game.spawn(i == 35 ? boss : kinds[i % 7]);
#else
    game.reset(0);
    Point wanted[] = {{174, 126}, {236, 126}, {350, 226}};
    for (int kind = 1; kind <= 3; ++kind) {
      int slot = -1;
      float closest = 1e9f;
      for (int i = 0; i < Slots; ++i) {
        float d = distance(game.slotPoint(i), wanted[kind - 1]);
        if (game.land(i) && !game.towers[i].obstacle && !game.towers[i].type &&
            d < closest) {
          closest = d;
          slot = i;
        }
      }
      if (slot >= 0)
        game.build(slot, kind);
    }
    for (int i = 0; i < 6; ++i)
      game.spawn(i == 5 ? 3 : i % 3);
#endif
    game.wave = 3;
    game.active = true;
    selected = -1;
    view = Play;
    fullRedraw = phase == 0;
    displayValid=false;
    pxa_log_write(2, phase == 0 ? "carrot probe full begin"
                                : "carrot probe regions begin");
#ifdef CARROT_PROFILE
    profiling = {};
    profiling.epoch = now;
#endif
  }
#ifdef CARROT_LIVE_PROBE
  constexpr int LiveUnits=
#ifdef CARROT_STRESS_PROBE
      36;
#else
      6;
#endif
  game.hearts=game.maxHearts;
  game.result=Result::None;
  game.active=true;
  game.remaining=0;
  // Replace killed/escaped enemies in different positions along the road.
  // Existing enemies and shots continue through the production simulation.
  int kinds[]={0,1,2,4,5,7,9};
  while(game.countEnemies()<LiveUnits){
    float progress=40+float(game.serial*73%1000)/1000*(game.pathLength()-100);
    game.spawn(game.serial%17==0?game.definition().boss>=0?game.definition().boss:3:kinds[game.serial%7],progress);
  }
  return;
#endif
  float time = float((now - epoch) % 18000000) / 1000000;
#ifdef CARROT_STRESS_PROBE
  constexpr int Units = 36;
#else
  constexpr int Units = 6;
#endif
  for (int i = 0; i < Units; ++i) {
    auto &e = game.enemies[i];
    e.progress = 40 + i * (game.pathLength() - 100) / Units + 18 * std::sin(time * 1.5f + i);
    auto nextPosition=game.position(e.progress);
    if(std::abs(nextPosition.x-e.p.x)>.0001f)e.facesRight=nextPosition.x>e.p.x;
    e.p = nextPosition;
    e.hp = e.maxhp * .7f;
    e.slow = i == 3 ? 2 : 0;
  }
  int tower = 0;
  for (int i = 0; i < Slots; ++i)
    if (game.towers[i].type) {
      auto &t = game.towers[i];
      Point p = game.slotPoint(i), aim = game.enemies[(tower * 5 + t.type) % Units].p;
      t.facing = std::atan2(aim.y - p.y, aim.x - p.x);
      t.pulse = std::max(0.f, .16f - std::fmod(time + t.type * .2f, .8f));
      auto &shot = game.shots[tower++];
      shot.active = true;
      shot.type = t.type;
      shot.level = t.level;
      shot.age = std::fmod(time, .5f);
      shot.duration = .5f;
      shot.start=p;
      shot.end=aim;
      float f = shot.age / shot.duration;
      shot.p = {p.x + (aim.x - p.x) * f, p.y + (aim.y - p.y) * f};
    }
}
