/* Pixel Dungeon game state: hero, mobs, items, turns, combat, progression.
 *
 * The model follows Shattered Pixel Dungeon's core loop in a simplified form:
 * a turn-based hero on a procedurally generated grid, melee combat with
 * accuracy/damage/armor rolls, XP and level-ups, hunger, loot and stairs that
 * lead to a deeper floor. */
#ifndef PD_GAME_H
#define PD_GAME_H

#include <stdint.h>

#include "assets.h"
#include "dungeon.h"

#define PD_MOBS_MAX 16
#define PD_GROUND_MAX 32
#define PD_BAG_MAX 12
#define PD_MESSAGE_COUNT 24
#define PD_MESSAGE_TEXT 44
#define PD_EFFECTS_MAX 24
#define PD_TILE_CHANGES_MAX 48
#define PD_SAVE_SLOTS 5
#define PD_RANK_COUNT 5
#define PD_HUNGER_MAX 320
#define PD_HUNGER_WARN 60

enum {
    PD_PHASE_TITLE = 0,
    PD_PHASE_PLAY,
    PD_PHASE_BAG,
    PD_PHASE_DEAD,
    PD_PHASE_WON,
    PD_PHASE_SAVES,
    PD_PHASE_CLASS,
    PD_PHASE_INFO,
    PD_PHASE_SETTINGS,
    PD_PHASE_PAUSE,
    PD_PHASE_RANKINGS,
    PD_PHASE_JOURNAL,
    PD_PHASE_SHOP,
    PD_PHASE_AMULET,
};

typedef struct {
    uint8_t occupied, cls, depth, level, deepest, kills;
    uint16_t gold, turns;
} pd_save_slot_t;

enum {
    PD_ITEM_GOLD = 0,
    PD_ITEM_POTION_HEAL,
    PD_ITEM_POTION_STRENGTH,
    PD_ITEM_SCROLL_UPGRADE,
    PD_ITEM_SCROLL_MAP,
    PD_ITEM_WEAPON,
    PD_ITEM_ARMOR,
    PD_ITEM_FOOD,
    PD_ITEM_IRON_KEY,
    PD_ITEM_AMULET,
    PD_ITEM_WAND_MAGIC,
};

enum {
    PD_MSG_NEUTRAL = 0,
    PD_MSG_GOOD,
    PD_MSG_BAD,
    PD_MSG_WARN,
    PD_MSG_INFO,
};

enum {
    PD_SOUND_HIT = 0,
    PD_SOUND_MISS,
    PD_SOUND_HIT_STRONG,
    PD_SOUND_DEATH,
    PD_SOUND_DRINK,
    PD_SOUND_EAT,
    PD_SOUND_GOLD,
    PD_SOUND_ITEM,
    PD_SOUND_READ,
    PD_SOUND_DESCEND,
    PD_SOUND_DOOR,
    PD_SOUND_TRAP,
    PD_SOUND_LEVELUP,
    PD_SOUND_SHATTER,
    PD_SOUND_UNLOCK,
    PD_SOUND_HUNGRY,
    PD_SOUND_STEP,
    PD_SOUND_GRASS,
    PD_SOUND_TRAMPLE,
    PD_SOUND_WATER,
    PD_SOUND_MAGIC,
    PD_SOUND_LIGHTNING,
    PD_SOUND_BLAST,
    PD_SOUND_COUNT,
};

enum {
    PD_EFFECT_DAMAGE = 0,
    PD_EFFECT_ZAP,
    PD_EFFECT_SPARK,
    PD_EFFECT_BLOOD,
    PD_EFFECT_LEAF,
    PD_EFFECT_SEARCH,
    PD_EFFECT_POISON,
    PD_EFFECT_TRAP_DART,
    PD_EFFECT_TRAP_BLAST,
    PD_EFFECT_LIGHTNING,
    PD_EFFECT_SWARM,
};

typedef struct {
    uint8_t kind;
    uint8_t tier;   /* equipment tier, or current wand charges */
    uint8_t level;  /* upgrade level */
    uint16_t gold;  /* gold amount for PD_ITEM_GOLD */
} pd_item_t;

typedef struct {
    uint8_t used;
    uint8_t x, y;
    pd_item_t item;
} pd_ground_t;

typedef struct {
    uint8_t cls;
    int16_t hp;
    int16_t max_hp;
    uint8_t str;
    uint8_t level;
    uint8_t xp;
    int16_t hunger;
    uint16_t gold;
    uint8_t keys;
    uint8_t poison;
    uint8_t x, y, facing;
    int8_t weapon; /* bag slot, -1 for fists */
    int8_t armor;  /* bag slot, -1 for rags */
} pd_hero_t;

typedef struct {
    uint8_t type; /* index into the mob table, 0xFF when empty */
    uint8_t x, y;
    int16_t hp;
    uint8_t awake;
    uint8_t wander;    /* turns until the next idle step */
    uint8_t dying;     /* frames left in the death animation, 0 when alive */
    uint8_t attacking; /* frames left in the attack pose */
    uint8_t from_x, from_y, moving;
} pd_mob_t;

typedef struct {
    char text[PD_MESSAGE_TEXT];
    uint8_t color;
} pd_message_t;

typedef struct {
    uint8_t kind;
    uint8_t x, y;
    int16_t value;
    uint8_t ttl;
} pd_effect_t;

typedef struct {
    uint8_t x, y;
    uint8_t tile;
} pd_tile_change_t;

typedef struct {
    uint32_t run_seed;
    uint32_t roll_rng; /* runtime combat/loot rolls, saved with the run */
    uint8_t depth;
    uint8_t lock_x, lock_y, shop_x, shop_y;
    uint8_t generation;
    uint8_t phase;
    uint8_t class_choice;
    uint8_t active_slot, selected_slot;
    uint8_t settings_from_title;
    pd_save_slot_t slots[PD_SAVE_SLOTS];
    pd_save_slot_t rankings[PD_RANK_COUNT];
    uint16_t turn;
    uint8_t kills;
    uint8_t deepest;
    pd_hero_t hero;
    pd_level_t level;
    pd_mob_t mobs[PD_MOBS_MAX];
    pd_item_t bag[PD_BAG_MAX];
    uint8_t bag_count;
    pd_ground_t ground[PD_GROUND_MAX];
    uint8_t ground_count;
    pd_message_t messages[PD_MESSAGE_COUNT];
    uint16_t message_total;
    pd_effect_t effects[PD_EFFECTS_MAX];
    pd_tile_change_t changes[PD_TILE_CHANGES_MAX];
    uint8_t change_count;
    /* Auto-walk target, stopped by a visible hostile or a new input. */
    uint8_t walk_active;
    uint8_t walk_attack;
    uint8_t walk_x, walk_y;
    uint8_t walk_steps;
    uint8_t hero_moving; /* frames left in the walk cycle */
    uint8_t hero_from_x, hero_from_y;
    int8_t bag_selected;
    int8_t wand_slot; /* -1 unless choosing a visible target */
    uint8_t potion_hint; /* frames left on the "no potions" flash */
    uint8_t sounds[8];
    uint8_t sound_count;
} pd_game_t;

void pd_game_reset(pd_game_t *game, uint32_t seed);
void pd_game_start_run(pd_game_t *game, uint8_t class_choice);
void pd_game_enter_depth(pd_game_t *game, uint8_t depth);

/* One hero turn plus the world's response. */
void pd_game_hero_step(pd_game_t *game, int dx, int dy);
void pd_game_hero_wait(pd_game_t *game);
void pd_game_hero_search(pd_game_t *game);
void pd_game_shop_buy(pd_game_t *game);
void pd_game_hero_stairs(pd_game_t *game);
void pd_game_amulet_exit(pd_game_t *game);
void pd_game_amulet_stay(pd_game_t *game);
void pd_game_hero_potion(pd_game_t *game);
void pd_game_wand_zap(pd_game_t *game, int x, int y);
void pd_game_tap(pd_game_t *game, int x, int y);

/* Bag operations. */
void pd_game_bag_use(pd_game_t *game, int slot);
void pd_game_bag_drop(pd_game_t *game, int slot);

/* Advances auto-walk and effect animation; call once per frame. */
void pd_game_tick(pd_game_t *game);

/* Takes up to `limit` queued sound ids; returns how many were written. */
int pd_game_take_sounds(pd_game_t *game, uint8_t *out, int limit);

/* Description helpers used by the renderer. */
const char *pd_item_name(const pd_item_t *item);
void pd_item_detail(const pd_item_t *item, char *out, int capacity);
const char *pd_mob_name(uint8_t type);
int pd_mob_max_hp(uint8_t type);
uint8_t pd_mob_sprite(uint8_t type);
uint8_t pd_item_sprite(const pd_item_t *item);
int pd_hero_armor_value(const pd_game_t *game);
uint8_t pd_hero_visual_tier(const pd_game_t *game);
void pd_hero_damage_range(const pd_game_t *game, int *low, int *high);
const char *pd_class_name(uint8_t cls);

/* Save/load through the Host key-value Storage service. */
int pd_game_serialize(const pd_game_t *game, uint8_t *out, int capacity);
int pd_game_restore(pd_game_t *game, const uint8_t *bytes, int length);
int pd_game_save_summary(const uint8_t *bytes, int length,
                         pd_save_slot_t *summary);
int pd_game_visible_save_slots(const pd_game_t *game,
                               uint8_t output[PD_SAVE_SLOTS]);

#endif
