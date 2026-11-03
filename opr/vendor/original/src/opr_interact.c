#include "opr.h"
#include <stdlib.h>
#include <stdio.h>

void opr_open_chest(OPR_GameState *g, int entityIndex) {
    // Simple loot table (expand later)
    int roll = rand() % 4;

    switch (roll) {
        case 0: // Weapon upgrade
            g->player.weaponAtk += 1;
            break;

        case 1: // Armor upgrade
            g->player.armorDef += 1;
            break;

        case 2: // Potions
            g->player.potions += 1 + (rand() % 2);
            break;

        case 3: // XP
            g->player.xp += 5 + (rand() % 6);
            break;
    }

    // Level-up check
    opr_check_levelup(g);
}

void opr_use_shrine(OPR_GameState *g) {
    // Heal fully
    g->player.hp = g->player.maxHp;

    // Small buff
    g->player.atk += 1;
    g->player.def += 1;
}
