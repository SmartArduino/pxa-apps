#include "unity.h"
#include "opr.h"

void test_interact(void) {
    OPR_GameState g;
    opr_init(&g, 0);

    int oldAtk = g.player.atk;

    // Fake chest
    g.entityCount = 1;
    g.entities[0].type = OPR_CHEST;

    opr_open_chest(&g, 0);

    TEST_ASSERT_TRUE(g.player.atk != oldAtk ||
                     g.player.armorDef != 0 ||
                     g.player.potions != 2 ||
                     g.player.xp != 0);
}
