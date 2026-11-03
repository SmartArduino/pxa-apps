#include "opr.h"
#include <stdlib.h>

//
// Forward declarations for helpers implemented in other modules
//
void opr_open_chest(OPR_GameState *g, int entityIndex);
void opr_use_shrine(OPR_GameState *g);
void opr_descend(OPR_GameState *g);

//
// Reveal fog around player
//
static void opr_reveal(OPR_GameState *g, int px, int py) {
    for (int dy = -2; dy <= 2; dy++) {
        for (int dx = -2; dx <= 2; dx++) {
            int nx = px + dx;
            int ny = py + dy;
            if (nx >= 0 && nx < OPR_COLS && ny >= 0 && ny < OPR_ROWS) {
                g->map.fog[ny][nx] = 0;
                g->map.visited[ny][nx] = 1;
            }
        }
    }
}

//
// Find entity at tile (returns index or -1)
//
static int find_entity(OPR_GameState *g, int x, int y) {
    for (int i = 0; i < g->entityCount; i++) {
        if (g->entities[i].x == x && g->entities[i].y == y)
            return i;
    }
    return -1;
}

//
// Main movement function
//
void opr_move(OPR_GameState *g, int dx, int dy) {
    if (g->inCombat || g->gameOver || g->victory)
        return;

    int nx = g->player.x + dx;
    int ny = g->player.y + dy;

    // Bounds check
    if (nx < 0 || nx >= OPR_COLS || ny < 0 || ny >= OPR_ROWS)
        return;

    // Wall check
    if (g->map.tiles[ny][nx] == 1)
        return;

    // Check entity collision
    int hit = find_entity(g, nx, ny);
    if (hit != -1) {
        OPR_Entity *e = &g->entities[hit];

        switch (e->type) {

        case OPR_ENEMY:
        case OPR_BOSS:
            // Enter combat mode
            g->inCombat = 1;
            g->combatIndex = hit;
            return;

        case OPR_CHEST:
            opr_open_chest(g, hit);
            // Remove chest
            g->entities[hit] = g->entities[g->entityCount - 1];
            g->entityCount--;
            break;

        case OPR_POTION:
            g->player.potions++;
            // Remove potion
            g->entities[hit] = g->entities[g->entityCount - 1];
            g->entityCount--;
            break;

        case OPR_STAIRS:
            opr_descend(g);
            return;

        case OPR_SHRINE:
            opr_use_shrine(g);
            // Remove shrine
            g->entities[hit] = g->entities[g->entityCount - 1];
            g->entityCount--;
            break;

        default:
            break;
        }
    }

    // Move player
    g->player.x = nx;
    g->player.y = ny;

    // Reveal fog around player
    opr_reveal(g, nx, ny);
}
