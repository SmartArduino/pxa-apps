#include "unity.h"
#include "opr.h"

void test_progress(void) {
    OPR_GameState g;
    opr_init(&g, 0);

    g.player.xp = g.player.xpNext;
    opr_check_levelup(&g);

    TEST_ASSERT_EQUAL(2, g.player.level);
    TEST_ASSERT_TRUE(g.player.maxHp > 30);
}
