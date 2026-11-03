#include "opr.h"
#include <stdlib.h>

//
// Forward declarations (implemented in opr_progress.c)
//
void opr_check_levelup(OPR_GameState *g);
void opr_trigger_victory(OPR_GameState *g);
void opr_trigger_gameover(OPR_GameState *g);

//
// Damage helpers
//
static int calc_player_damage(OPR_GameState *g, OPR_Entity *e) {
    int base = g->player.atk + g->player.weaponAtk;
    int dmg = base - e->def + (rand() % 3);
    if (dmg < 1) dmg = 1;
    return dmg;
}

static int calc_enemy_damage(OPR_GameState *g, OPR_Entity *e) {
    int base = e->atk;
    int def = g->player.def + g->player.armorDef;
    int dmg = base - def + (rand() % 2);
    if (dmg < 1) dmg = 1;
    return dmg;
}

//
// End combat cleanly
//
static void opr_end_combat(OPR_GameState *g) {
    g->inCombat = 0;
    g->combatIndex = -1;
}

//
// Player ATTACK action
//
void opr_combat_attack(OPR_GameState *g) {
    if (!g->inCombat || g->combatIndex < 0)
        return;

    OPR_Entity *e = &g->entities[g->combatIndex];

    // Player hits enemy
    int pDmg = calc_player_damage(g, e);
    e->hp -= pDmg;

    // Enemy dies
    if (e->hp <= 0) {
        g->player.xp += e->xp;

        // Remove entity
        g->entities[g->combatIndex] = g->entities[g->entityCount - 1];
        g->entityCount--;

        // End combat
        opr_end_combat(g);

        // Level up check
        opr_check_levelup(g);

        // Boss victory
        if (e->type == OPR_BOSS)
            opr_trigger_victory(g);

        return;
    }

    // Enemy counterattacks
    int eDmg = calc_enemy_damage(g, e);
    g->player.hp -= eDmg;

    // Player dies
    if (g->player.hp <= 0) {
        g->player.hp = 0;
        opr_end_combat(g);
        opr_trigger_gameover(g);
        return;
    }
}

//
// Player POTION action
//
void opr_combat_potion(OPR_GameState *g) {
    if (!g->inCombat || g->combatIndex < 0)
        return;

    if (g->player.potions <= 0)
        return;

    OPR_Entity *e = &g->entities[g->combatIndex];

    // Use potion
    g->player.potions--;
    int heal = 8 + g->player.level * 2;
    g->player.hp += heal;
    if (g->player.hp > g->player.maxHp)
        g->player.hp = g->player.maxHp;

    // Enemy hits after potion
    int eDmg = calc_enemy_damage(g, e);
    g->player.hp -= eDmg;

    // Player dies
    if (g->player.hp <= 0) {
        g->player.hp = 0;
        opr_end_combat(g);
        opr_trigger_gameover(g);
        return;
    }
}

//
// Player FLEE action
//
void opr_combat_flee(OPR_GameState *g) {
    if (!g->inCombat || g->combatIndex < 0)
        return;

    OPR_Entity *e = &g->entities[g->combatIndex];

    float chance = 0.4f + g->player.spd * 0.05f;
    float roll = (float)rand() / (float)RAND_MAX;

    // Successful flee
    if (roll < chance) {
        opr_end_combat(g);
        return;
    }

    // Failed flee → enemy hits
    int eDmg = calc_enemy_damage(g, e);
    g->player.hp -= eDmg;

    if (g->player.hp <= 0) {
        g->player.hp = 0;
        opr_end_combat(g);
        opr_trigger_gameover(g);
        return;
    }
}
