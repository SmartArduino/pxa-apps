/* SPDX-License-Identifier: MIT. Original engine owns all combat/progression values. */
#include "session.h"
#include "portable-rng.h"
#include <string.h>
#include <stdlib.h>
_Static_assert(RAND_MAX == 2147483647, "OPR flee probability expects this rand range");
uint32_t opr_state_hash(const OPR_GameState *g) {
    const unsigned char *p = (const unsigned char *)g;
    uint32_t h = UINT32_C(2166136261);
    for (unsigned i=0; i<sizeof(*g); ++i) h=(h^p[i])*UINT32_C(16777619);
    return h;
}
void opr_session_start(opr_session *s, int class_id, uint32_t seed) {
    memset(s, 0, sizeof(*s)); s->seed=seed; opr_rng_seed(seed);
    opr_init(&s->game, class_id); s->active=1;
}
void opr_session_pause(opr_session *s, int paused) { s->paused=!!paused; }
int opr_session_action(opr_session *s, opr_action action) {
    if (!s->active || s->paused || s->game.gameOver || s->game.victory) return 0;
    uint32_t before=opr_state_hash(&s->game), rng=opr_rng_state();
    switch(action) {
      case OPR_ACT_UP: opr_move(&s->game,0,-1); break;
      case OPR_ACT_DOWN: opr_move(&s->game,0,1); break;
      case OPR_ACT_LEFT: opr_move(&s->game,-1,0); break;
      case OPR_ACT_RIGHT: opr_move(&s->game,1,0); break;
      case OPR_ACT_ATTACK: opr_combat_attack(&s->game); break;
      case OPR_ACT_POTION: opr_combat_potion(&s->game); break;
      case OPR_ACT_FLEE: opr_combat_flee(&s->game); break;
      default: return 0;
    }
    if (before==opr_state_hash(&s->game) && rng==opr_rng_state()) return 0;
    s->actions++; return 1;
}
