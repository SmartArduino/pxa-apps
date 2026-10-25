// SPDX-License-Identifier: MIT
#include "../vendor/src/games/gameGrapplingH.c"
int test_grappling_attached(void) { return player.attachedAnchor != NULL; }
float test_grappling_y(void) { return player.pos.y; }
void test_grappling_offer_anchor(void) {
  // A deterministic reachable anchor after the real release/release-input cycle.
  anchorPoints[1].pos = player.pos;
  anchorPoints[1].pos.y -= 10;
  anchorPoints[1].isAlive = true;
}
