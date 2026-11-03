/* SPDX-License-Identifier: MIT. Turn/lifecycle adapter, no duplicate game rules. */
#ifndef OPR_PXA_SESSION_H
#define OPR_PXA_SESSION_H
#include <stdint.h>
#include "opr.h"
typedef enum { OPR_ACT_UP, OPR_ACT_DOWN, OPR_ACT_LEFT, OPR_ACT_RIGHT,
               OPR_ACT_ATTACK, OPR_ACT_POTION, OPR_ACT_FLEE } opr_action;
typedef struct { OPR_GameState game; uint32_t seed, actions; int active, paused; } opr_session;
void opr_session_start(opr_session *s, int class_id, uint32_t seed);
int opr_session_action(opr_session *s, opr_action action);
void opr_session_pause(opr_session *s, int paused);
uint32_t opr_state_hash(const OPR_GameState *g);
#endif
