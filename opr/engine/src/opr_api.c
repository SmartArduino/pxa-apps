#include "opr.h"

static OPR_GameState G;

#ifdef __cplusplus
extern "C" {
#endif

void opr_web_init(int classId) {
    opr_init(&G, classId);
}

OPR_GameState* opr_web_get_state() {
    return &G;
}

void opr_web_move(int dx, int dy) {
    opr_move(&G, dx, dy);
}

void opr_web_attack() {
    opr_combat_attack(&G);
}

void opr_web_potion() {
    opr_combat_potion(&G);
}

void opr_web_flee() {
    opr_combat_flee(&G);
}

#ifdef __cplusplus
}
#endif
