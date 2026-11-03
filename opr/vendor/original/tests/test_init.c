#include "unity.h"
#include "opr.h"

void test_init(void) {
    OPR_GameState g;
    opr_init(&g, 0);

    TEST_ASSERT_EQUAL(1, g.player.level);
    TEST_ASSERT_EQUAL(0, g.player.xp);
    TEST_ASSERT_EQUAL(10, g.player.xpNext);
    TEST_ASSERT_EQUAL(2, g.player.potions);
    TEST_ASSERT_EQUAL(1, g.player.floor);

    TEST_ASSERT_EQUAL(0, g.inCombat);
    TEST_ASSERT_EQUAL(-1, g.combatIndex);
    TEST_ASSERT_EQUAL(0, g.gameOver);
    TEST_ASSERT_EQUAL(0, g.victory);

    // Map should be generated
    int walls = 0;
    for (int r = 0; r < OPR_ROWS; r++)
        for (int c = 0; c < OPR_COLS; c++)
            walls += g.map.tiles[r][c] == 1;

    TEST_ASSERT_TRUE(walls < OPR_ROWS * OPR_COLS); // some floors exist
}
