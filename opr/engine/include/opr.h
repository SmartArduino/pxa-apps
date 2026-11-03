#ifndef OPR_H
#define OPR_H

#define OPR_COLS 16
#define OPR_ROWS 16
#define OPR_MAX_ENTITIES 64

typedef enum {
    OPR_NONE,
    OPR_ENEMY,
    OPR_BOSS,
    OPR_CHEST,
    OPR_POTION,
    OPR_STAIRS,
    OPR_SHRINE
} OPR_EntityType;

typedef struct {
    int x, y;
    OPR_EntityType type;
    int hp, maxHp;
    int atk, def;
    int xp;
    int tier;
} OPR_Entity;

typedef struct {
    int tiles[OPR_ROWS][OPR_COLS];   // 0=floor, 1=wall
    int fog[OPR_ROWS][OPR_COLS];     // bool
    int visited[OPR_ROWS][OPR_COLS]; // bool
} OPR_Map;

typedef struct {
    int x, y;
    int hp, maxHp;
    int atk, def, spd;
    int level, xp, xpNext;
    int weaponAtk, armorDef;
    int potions;
    int floor;
    int classId;
} OPR_Player;

typedef struct {
    OPR_Player player;
    OPR_Map map;
    OPR_Entity entities[OPR_MAX_ENTITIES];
    int entityCount;

    int inCombat;
    int combatIndex;

    int gameOver;
    int victory;
} OPR_GameState;

// Initialization
void opr_init(OPR_GameState *g, int classId);

// Floor generation
void opr_generate_floor(OPR_GameState *g);

// Movement & interaction
void opr_move(OPR_GameState *g, int dx, int dy);

// Combat
void opr_combat_attack(OPR_GameState *g);
void opr_combat_potion(OPR_GameState *g);
void opr_combat_flee(OPR_GameState *g);

// Progression
void opr_descend(OPR_GameState *g);
void opr_check_levelup(OPR_GameState *g);
void opr_trigger_victory(OPR_GameState *g);
void opr_trigger_gameover(OPR_GameState *g);


#endif
