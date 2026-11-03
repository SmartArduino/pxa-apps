#include "unity.h"
#include "opr.h"

void test_move(void) {
    OPR_GameState g;
    opr_init(&g, 0);

    int px = g.player.x;
    int py = g.player.y;

    // Try moving into a wall
    opr_move(&g, -1, 0);
    TEST_ASSERT_EQUAL(px, g.player.x);
    TEST_ASSERT_EQUAL(py, g.player.y);

    // Try moving into a floor tile
    opr_move(&g, 1, 0);
    TEST_ASSERT_EQUAL(px + 1, g.player.x);
}
