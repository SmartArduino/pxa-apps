#include "opr.h"
#include <string.h>

//
// Internal class table
//
typedef struct {
    int hp, atk, def, spd;
    const char *name;
} OPR_ClassTemplate;

static const OPR_ClassTemplate OPR_CLASSES[] = {
    { 30, 5, 3, 2, "Warrior" },
    { 20, 6, 1, 5, "Rogue" },
    { 18, 8, 1, 3, "Mage" }
};

//
// Reset map arrays to default values
//
static void opr_clear_map(OPR_Map *m) {
    for (int r = 0; r < OPR_ROWS; r++) {
        for (int c = 0; c < OPR_COLS; c++) {
            m->tiles[r][c]   = 1;  // wall
            m->fog[r][c]     = 1;  // hidden
            m->visited[r][c] = 0;  // not visited
        }
    }
}

//
// Reset entity list
//
static void opr_clear_entities(OPR_GameState *g) {
    g->entityCount = 0;
    for (int i = 0; i < OPR_MAX_ENTITIES; i++) {
        g->entities[i].type = OPR_NONE;
    }
}

//
// Initialize player stats from classId
//
static void opr_init_player(OPR_Player *p, int classId) {
    if (classId < 0 || classId > 2)
        classId = 0;

    const OPR_ClassTemplate *cls = &OPR_CLASSES[classId];

    p->classId = classId;
    p->x = 0;
    p->y = 0;

    p->hp = cls->hp;
    p->maxHp = cls->hp;
    p->atk = cls->atk;
    p->def = cls->def;
    p->spd = cls->spd;

    p->level = 1;
    p->xp = 0;
    p->xpNext = 10;

    p->weaponAtk = 0;
    p->armorDef = 0;

    p->potions = 2;
    p->floor = 1;
}

//
// Public API: initialize the entire game state
//
void opr_init(OPR_GameState *g, int classId) {
    memset(g, 0, sizeof(OPR_GameState));

    // Player
    opr_init_player(&g->player, classId);

    // Map + fog + visited
    opr_clear_map(&g->map);

    // Entities
    opr_clear_entities(g);

    // Flags
    g->inCombat = 0;
    g->combatIndex = -1;
    g->gameOver = 0;
    g->victory = 0;

    // Generate first floor
    opr_generate_floor(g);
}
