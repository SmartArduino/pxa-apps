#include "unity.h"
#include "opr.h"

void test_combat(void) {
    OPR_GameState g;
    opr_init(&g, 0);

    // Create a dummy enemy
    g.entityCount = 1;
    g.entities[0].type = OPR_ENEMY;
    g.entities[0].x = g.player.x + 1;
    g.entities[0].y = g.player.y;
    g.entities[0].hp = 5;
    g.entities[0].maxHp = 5;
    g.entities[0].atk = 1;
    g.entities[0].def = 0;
    g.entities[0].xp = 3;

    // Enter combat
    opr_move(&g, 1, 0);
    TEST_ASSERT_TRUE(g.inCombat);

    int oldHp = g.entities[0].hp;
    opr_combat_attack(&g);
    TEST_ASSERT_TRUE(g.entities[0].hp < oldHp);
}
