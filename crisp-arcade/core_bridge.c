// SPDX-License-Identifier: MIT
// Including the exact upstream translation unit lets these small accessors use
// its private state without changing any vendored bytes or game rules.
#include "vendor/src/lib/cglp.c"
#include "core_bridge.h"

void crisp_core_init(void) {
  gameCount = 0;
  initGame();
}
void crisp_core_start(int index, int best, uint32_t seed) {
  initInput();
  restartGame(index + 1);
  // Keep upstream generated action effects, but never start its background score.
  isBgmEnabled = false;
  isPlayingBgm = false;
  setRandomSeed(&gameSeedRandom, seed);
  hiScore = best;
  // The PXA launcher supplies titles/instructions. Keep the game-over screen
  // until a deliberate retry, instead of running an upstream attract replay.
  hasTitle = false;
  // The PXA draw hook supplies localized score/game-over labels. Suppress the
  // upstream ASCII-only labels without altering any vendored source or physics.
  isShowingScore = false;
  gameOverText = "";
  initInGame();
  ticks = 0;
}
int crisp_core_state(void) { return state; }
void crisp_core_best(int best) { hiScore = best; }
