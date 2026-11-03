#include "unity.h"
#include "opr.h"

void test_floor(void) {
    OPR_GameState g;
    opr_init(&g, 0);

    // Floor must contain at least one entity (stairs or boss)
    TEST_ASSERT_TRUE(g.entityCount > 0);

    int floors = 0;
    for (int r = 0; r < OPR_ROWS; r++)
        for (int c = 0; c < OPR_COLS; c++)
            floors += g.map.tiles[r][c] == 0;

    TEST_ASSERT_TRUE(floors > 0);
}
