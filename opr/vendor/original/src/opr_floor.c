#include "opr.h"
#include <stdlib.h>
#include <string.h>

//
// --- Utility random helpers ---
//
static int rnd(int max) {
    return rand() % max;
}

static int rnd_range(int min, int max) {
    return min + (rand() % (max - min + 1));
}

//
// --- Reveal fog around player (radius 2) ---
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
// --- Room structure ---
//
typedef struct {
    int x, y, w, h;
} Room;

//
// --- Check room overlap ---
//
static int rooms_overlap(Room *a, Room *b) {
    return (
        a->x <= b->x + b->w + 1 &&
        a->x + a->w >= b->x - 1 &&
        a->y <= b->y + b->h + 1 &&
        a->y + a->h >= b->y - 1
    );
}

//
// --- Carve corridor between rooms ---
//
static void carve_corridor(OPR_Map *m, Room *a, Room *b) {
    int ax = a->x + a->w / 2;
    int ay = a->y + a->h / 2;
    int bx = b->x + b->w / 2;
    int by = b->y + b->h / 2;

    int cx = ax, cy = ay;

    while (cx != bx) {
        m->tiles[cy][cx] = 0;
        cx += (cx < bx) ? 1 : -1;
    }
    while (cy != by) {
        m->tiles[cy][cx] = 0;
        cy += (cy < by) ? 1 : -1;
    }
    m->tiles[cy][cx] = 0;
}

//
// --- Find random floor tile inside rooms ---
//
static int random_floor(OPR_GameState *g, Room *rooms, int roomCount, int *outX, int *outY) {
    for (int attempt = 0; attempt < 50; attempt++) {
        int ri = 1 + rnd(roomCount - 1);
        Room *rm = &rooms[ri];

        int x = rm->x + rnd(rm->w);
        int y = rm->y + rnd(rm->h);

        if (g->map.tiles[y][x] != 0)
            continue;

        // Check entity collision
        int occ = 0;
        for (int i = 0; i < g->entityCount; i++) {
            if (g->entities[i].x == x && g->entities[i].y == y) {
                occ = 1;
                break;
            }
        }
        if (!occ) {
            *outX = x;
            *outY = y;
            return 1;
        }
    }
    return 0;
}

//
// --- Add entity ---
//
static void add_entity(OPR_GameState *g, OPR_EntityType type, int x, int y) {
    if (g->entityCount >= OPR_MAX_ENTITIES)
        return;

    OPR_Entity *e = &g->entities[g->entityCount++];
    memset(e, 0, sizeof(OPR_Entity));

    e->type = type;
    e->x = x;
    e->y = y;
}

//
// --- Generate floor (main function) ---
//
void opr_generate_floor(OPR_GameState *g) {
    int f = g->player.floor;

    // Reset map
    for (int r = 0; r < OPR_ROWS; r++) {
        for (int c = 0; c < OPR_COLS; c++) {
            g->map.tiles[r][c] = 1;   // wall
            g->map.fog[r][c] = 1;
            g->map.visited[r][c] = 0;
        }
    }

    // Reset entities
    g->entityCount = 0;

    //
    // --- Room generation ---
    //
    Room rooms[32];
    int roomCount = 0;

    int numRooms = 5 + (f < 5 ? f : 5);

    for (int i = 0; i < numRooms * 10 && roomCount < numRooms; i++) {
        Room rm;
        rm.w = rnd_range(3, 6);
        rm.h = rnd_range(3, 6);
        rm.x = rnd_range(1, OPR_COLS - rm.w - 2);
        rm.y = rnd_range(1, OPR_ROWS - rm.h - 2);

        int overlap = 0;
        for (int j = 0; j < roomCount; j++) {
            if (rooms_overlap(&rm, &rooms[j])) {
                overlap = 1;
                break;
            }
        }
        if (!overlap) {
            rooms[roomCount++] = rm;

            for (int y = rm.y; y < rm.y + rm.h; y++)
                for (int x = rm.x; x < rm.x + rm.w; x++)
                    g->map.tiles[y][x] = 0;
        }
    }

    //
    // --- Connect rooms ---
    //
    for (int i = 1; i < roomCount; i++) {
        carve_corridor(&g->map, &rooms[i - 1], &rooms[i]);
    }

    //
    // --- Player start position ---
    //
    Room *start = &rooms[0];
    g->player.x = start->x + start->w / 2;
    g->player.y = start->y + start->h / 2;

    //
    // --- End room (stairs or boss) ---
    //
    Room *end = &rooms[roomCount - 1];
    int sx = end->x + end->w / 2;
    int sy = end->y + end->h / 2;

    if (f >= 5) {
        // Boss
        add_entity(g, OPR_BOSS, sx, sy);
        OPR_Entity *b = &g->entities[g->entityCount - 1];
        b->hp = 120 + f * 10;
        b->maxHp = b->hp;
        b->atk = 14 + f;
        b->def = 6;
        b->xp = 100;
    } else {
        add_entity(g, OPR_STAIRS, sx, sy);
    }

    //
    // --- Enemies ---
    //
    int maxTier = (int)((f / 1.2f) + 1);
    if (maxTier > 4) maxTier = 4;

    int numEnemies = 4 + f * 2;

    for (int i = 0; i < numEnemies; i++) {
        int ex, ey;
        if (!random_floor(g, rooms, roomCount, &ex, &ey))
            continue;

        add_entity(g, OPR_ENEMY, ex, ey);
        OPR_Entity *e = &g->entities[g->entityCount - 1];

        float scale = 1.0f + (f - 1) * 0.15f;

        // Simple scaling (you can expand this later)
        e->hp = (int)(10 * scale);
        e->maxHp = e->hp;
        e->atk = (int)(3 * scale);
        e->def = 1;
        e->xp = (int)(5 * scale);
        e->tier = maxTier;
    }

    //
    // --- Chests ---
    //
    int numChests = 2 + (f / 2);
    for (int i = 0; i < numChests; i++) {
        int cx, cy;
        if (random_floor(g, rooms, roomCount, &cx, &cy))
            add_entity(g, OPR_CHEST, cx, cy);
    }

    //
    // --- Potions ---
    //
    int numPotions = 2 + rnd(2);
    for (int i = 0; i < numPotions; i++) {
        int px, py;
        if (random_floor(g, rooms, roomCount, &px, &py))
            add_entity(g, OPR_POTION, px, py);
    }

    //
    // --- Shrine (50% chance) ---
    //
    if (rnd(100) < 50) {
        int sx2, sy2;
        if (random_floor(g, rooms, roomCount, &sx2, &sy2))
            add_entity(g, OPR_SHRINE, sx2, sy2);
    }

    //
    // --- Reveal around player ---
    //
    opr_reveal(g, g->player.x, g->player.y);
}
