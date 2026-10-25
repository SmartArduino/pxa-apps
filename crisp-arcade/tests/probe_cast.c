// SPDX-License-Identifier: MIT
// Test-only same-translation-unit probes; the game source remains unchanged.
#include "../vendor/src/games/gameCastN.c"
int test_cast_state(void) { return nodeState; }
float test_cast_power(void) { return throwPower; }
float test_cast_x(void) { return nodes[0].pos.x; }
