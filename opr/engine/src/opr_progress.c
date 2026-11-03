#include "opr.h"
#include <stdio.h>

void opr_check_levelup(OPR_GameState *g) {
    while (g->player.xp >= g->player.xpNext) {

        g->player.xp -= g->player.xpNext;
        g->player.level++;

        // Increase next XP threshold
        g->player.xpNext = (int)(g->player.xpNext * 1.6f);

        // Stat increases
        g->player.maxHp += 4;
        g->player.hp = g->player.maxHp;

        g->player.atk += 1;
        g->player.def += 1;

        // Platform layer could show "LEVEL UP!"
    }
}

void opr_descend(OPR_GameState *g) {
    g->player.floor++;

    // Heal on descent
    int heal = 5 + g->player.level;
    g->player.hp += heal;
    if (g->player.hp > g->player.maxHp)
        g->player.hp = g->player.maxHp;

    // Regenerate floor
    opr_generate_floor(g);

    // Platform layer could show "Descending to Floor X"
}

void opr_trigger_victory(OPR_GameState *g) {
    g->victory = 1;
    g->inCombat = 0;
    g->combatIndex = -1;

    // Platform layer could show victory screen
}

void opr_trigger_gameover(OPR_GameState *g) {
    g->gameOver = 1;
    g->inCombat = 0;
    g->combatIndex = -1;

    // Platform layer could show game over screen
}
