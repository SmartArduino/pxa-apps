#include "game.h"

#include <stddef.h>

#include "assets.h"
#include "strings.h"

/* ---------------------------------------------------------------------- */
/* Tables                                                                  */
/* ---------------------------------------------------------------------- */

typedef struct {
    pd_string_id_t name;
    uint8_t str;
    uint8_t hp;
    uint8_t dodge;
    uint8_t weapon_tier;
    uint8_t armor_tier;
    uint8_t potions;
} pd_class_def_t;

static const pd_class_def_t kClasses[3] = {
    {PD_STR_CLASS_1, 11, 22, 4, 1, 1, 1},
    {PD_STR_CLASS_2, 10, 18, 12, 1, 0, 2},
    {PD_STR_CLASS_3, 10, 16, 6, 2, 0, 3},
};

typedef struct {
    pd_string_id_t name;
    uint8_t sprite;
    uint8_t hp;
    uint8_t dmg_min;
    uint8_t dmg_max;
    uint8_t armor;
    uint8_t accuracy;
    uint8_t evasion;
    uint8_t xp;
    uint8_t min_depth;
    uint8_t max_depth;
} pd_mob_def_t;

static const pd_mob_def_t kMobs[] = {
    /* Order must match the sprite atlas: PD_SPRITE_MOB(type, frame). */
    {PD_STR_MOB_RAT, 0, 7, 1, 3, 0, 68, 5, 2, 1, 5},
    {PD_STR_MOB_GNOLL, 1, 11, 2, 5, 1, 72, 7, 4, 1, 8},
    {PD_STR_MOB_CRAB, 2, 18, 3, 6, 3, 68, 3, 6, 6, 11},
    {PD_STR_MOB_SKELETON, 3, 20, 4, 8, 2, 76, 8, 8, 6, 14},
    {PD_STR_MOB_BAT, 4, 18, 4, 8, 1, 72, 16, 8, 11, 18},
    {PD_STR_MOB_SNAKE, 5, 14, 3, 6, 1, 76, 12, 5, 3, 10},
    {PD_STR_MOB_SPINNER, 6, 26, 5, 10, 3, 78, 14, 12, 11, 20},
    {PD_STR_MOB_SLIME, 7, 16, 2, 6, 1, 70, 4, 4, 1, 8},
    {PD_STR_MOB_GOLEM, 8, 44, 8, 14, 6, 82, 6, 20, 16, 25},
    {PD_STR_MOB_YOG, 9, 110, 11, 19, 7, 88, 10, 60, 25, 25},
    {PD_STR_MOB_SWARM, 10, 22, 1, 4, 0, 72, 5, 3, 3, 7},
    {PD_STR_MOB_DM100, 11, 20, 2, 8, 2, 76, 8, 6, 6, 12},
};
#define PD_MOB_TYPE_COUNT ((int)(sizeof(kMobs) / sizeof(kMobs[0])))
#define PD_MOB_SPINNER 6
#define PD_MOB_DEMON 9
#define PD_MOB_SWARM 10
#define PD_MOB_DM100 11
#define PD_MOB_AWAKE 1
#define PD_MOB_SPLIT 2
#define PD_MOB_NO_XP 4
#define PD_MOB_SPLIT_WAIT 8

static const pd_string_id_t kWeaponNames[4] = {
    PD_STR_WEAPON_1, PD_STR_WEAPON_2, PD_STR_WEAPON_3, PD_STR_WEAPON_4};
static const uint8_t kWeaponMin[4] = {2, 3, 4, 6};
static const uint8_t kWeaponMax[4] = {4, 6, 9, 12};
static const pd_string_id_t kArmorNames[4] = {
    PD_STR_ARMOR_1, PD_STR_ARMOR_2, PD_STR_ARMOR_3, PD_STR_ARMOR_4};
static const uint8_t kArmorValue[4] = {2, 3, 5, 8};

/* ---------------------------------------------------------------------- */
/* Small text helpers (no libc)                                            */
/* ---------------------------------------------------------------------- */

typedef struct {
    char text[PD_MESSAGE_TEXT];
    int length;
} pd_text_t;

static void text_init(pd_text_t *text) {
    text->length = 0;
    text->text[0] = '\0';
}

static void text_add(pd_text_t *text, const char *value) {
    while (*value != '\0' && text->length < PD_MESSAGE_TEXT - 1)
        text->text[text->length++] = *value++;
    text->text[text->length] = '\0';
}

static void text_add_int(pd_text_t *text, int value) {
    char digits[12];
    int count = 0;
    if (value < 0) {
        text_add(text, "-");
        value = -value;
    }
    if (value == 0) {
        text_add(text, "0");
        return;
    }
    while (value > 0 && count < 11) {
        digits[count++] = (char)('0' + value % 10);
        value /= 10;
    }
    while (count > 0) {
        char single[2];
        single[0] = digits[--count];
        single[1] = '\0';
        text_add(text, single);
    }
}


/* ---------------------------------------------------------------------- */
/* Messages and effects                                                    */
/* ---------------------------------------------------------------------- */

static void message_push(pd_game_t *game, uint8_t color, const pd_text_t *text) {
    pd_message_t *slot =
        &game->messages[game->message_total % PD_MESSAGE_COUNT];
    int index = 0;
    while (index < PD_MESSAGE_TEXT - 1 && text->text[index] != '\0') {
        slot->text[index] = text->text[index];
        ++index;
    }
    slot->text[index] = '\0';
    slot->color = color;
    ++game->message_total;
}

static pd_string_id_t item_name_id(const pd_item_t *item);

static void message_simple(pd_game_t *game, uint8_t color,
                           pd_string_id_t id) {
    pd_text_t line;
    pd_str_format(line.text, PD_MESSAGE_TEXT, id);
    line.length = 0;
    message_push(game, color, &line);
}

/* Localized message helpers. The variadic formatter takes its arguments in
 * placeholder order, so these wrappers match each string's shape. */
static void message_num(pd_game_t *game, uint8_t color, pd_string_id_t id,
                        int value) {
    pd_text_t line;
    pd_str_format(line.text, PD_MESSAGE_TEXT, id, value);
    line.length = 0;
    message_push(game, color, &line);
}

static void message_text(pd_game_t *game, uint8_t color, pd_string_id_t id,
                         const char *value) {
    pd_text_t line;
    pd_str_format(line.text, PD_MESSAGE_TEXT, id, value);
    line.length = 0;
    message_push(game, color, &line);
}

static void message_text_num(pd_game_t *game, uint8_t color,
                             pd_string_id_t id, const char *value, int number) {
    pd_text_t line;
    pd_str_format(line.text, PD_MESSAGE_TEXT, id, value, number);
    line.length = 0;
    message_push(game, color, &line);
}

static void message_item(pd_game_t *game, uint8_t color, pd_string_id_t id,
                         const pd_item_t *item) {
    char name[PD_MESSAGE_TEXT];
    pd_text_t line;
    pd_str_item(name, sizeof(name), item_name_id(item), item->level);
    pd_str_format(line.text, PD_MESSAGE_TEXT, id, name);
    line.length = 0;
    message_push(game, color, &line);
}

static void sound_add(pd_game_t *game, uint8_t sound) {
    if (game->sound_count >= sizeof(game->sounds)) return;
    game->sounds[game->sound_count++] = sound;
}

int pd_game_take_sounds(pd_game_t *game, uint8_t *out, int limit) {
    int count = game->sound_count < limit ? (int)game->sound_count : limit;
    for (int index = 0; index < count; ++index) out[index] = game->sounds[index];
    for (int index = count; index < game->sound_count; ++index)
        game->sounds[index - count] = game->sounds[index];
    game->sound_count = (uint8_t)(game->sound_count - count);
    return count;
}

static void effect_add(pd_game_t *game, uint8_t kind, int x, int y, int value) {
    for (int index = 0; index < PD_EFFECTS_MAX; ++index) {
        if (game->effects[index].ttl != 0) continue;
        game->effects[index].kind = kind;
        game->effects[index].x = (uint8_t)x;
        game->effects[index].y = (uint8_t)y;
        game->effects[index].value = (int16_t)value;
        game->effects[index].ttl = kind == PD_EFFECT_LEAF ? 40 : 20;
        return;
    }
}

static void record_change(pd_game_t *game, int x, int y, uint8_t tile) {
    if (game->change_count >= PD_TILE_CHANGES_MAX) return;
    game->changes[game->change_count].x = (uint8_t)x;
    game->changes[game->change_count].y = (uint8_t)y;
    game->changes[game->change_count].tile = tile;
    ++game->change_count;
}

/* ---------------------------------------------------------------------- */
/* Item helpers                                                            */
/* ---------------------------------------------------------------------- */

static pd_string_id_t item_name_id(const pd_item_t *item) {
    const uint8_t tier = item->tier < 4 ? item->tier : 3;
    switch (item->kind) {
        case PD_ITEM_GOLD:
            return PD_STR_ITEM_GOLD;
        case PD_ITEM_IRON_KEY:
            return PD_STR_ITEM_IRON_KEY;
        case PD_ITEM_AMULET:
            return PD_STR_ITEM_AMULET;
        case PD_ITEM_WAND_MAGIC:
            return PD_STR_ITEM_WAND_MAGIC;
        case PD_ITEM_POTION_HEAL:
            return PD_STR_ITEM_POTION_HEAL;
        case PD_ITEM_POTION_STRENGTH:
            return PD_STR_ITEM_POTION_STRENGTH;
        case PD_ITEM_SCROLL_UPGRADE:
            return PD_STR_ITEM_SCROLL_UPGRADE;
        case PD_ITEM_SCROLL_MAP:
            return PD_STR_ITEM_SCROLL_MAP;
        case PD_ITEM_WEAPON:
            return kWeaponNames[tier];
        case PD_ITEM_ARMOR:
            return kArmorNames[tier];
        default:
            return PD_STR_ITEM_FOOD;
    }
}

const char *pd_item_name(const pd_item_t *item) {
    return pd_str(item_name_id(item));
}

void pd_item_detail(const pd_item_t *item, char *out, int capacity) {
    const uint8_t tier = item->tier < 4 ? item->tier : 3;
    switch (item->kind) {
        case PD_ITEM_GOLD:
            pd_str_format(out, capacity, PD_STR_DETAIL_GOLD, (int)item->gold);
            break;
        case PD_ITEM_AMULET:
            pd_str_format(out, capacity, PD_STR_DETAIL_AMULET);
            break;
        case PD_ITEM_WAND_MAGIC:
            pd_str_format(out, capacity, PD_STR_DETAIL_WAND_MAGIC,
                          (int)item->tier, 3 + (int)item->level);
            break;
        case PD_ITEM_POTION_HEAL:
            pd_str_format(out, capacity, PD_STR_DETAIL_HEAL,
                          18 + item->level * 6);
            break;
        case PD_ITEM_POTION_STRENGTH:
            pd_str_format(out, capacity, PD_STR_DETAIL_STRENGTH);
            break;
        case PD_ITEM_SCROLL_UPGRADE:
            pd_str_format(out, capacity, PD_STR_DETAIL_UPGRADE);
            break;
        case PD_ITEM_SCROLL_MAP:
            pd_str_format(out, capacity, PD_STR_DETAIL_MAP);
            break;
        case PD_ITEM_WEAPON:
            pd_str_format(out, capacity, PD_STR_DETAIL_WEAPON,
                          kWeaponMin[tier] + item->level,
                          kWeaponMax[tier] + item->level);
            break;
        case PD_ITEM_ARMOR:
            pd_str_format(out, capacity, PD_STR_DETAIL_ARMOR,
                          kArmorValue[tier] + item->level / 2);
            break;
        default:
            pd_str_format(out, capacity, PD_STR_DETAIL_FOOD);
            break;
    }
}

const char *pd_mob_name(uint8_t type) {
    return pd_str(type < PD_MOB_TYPE_COUNT ? kMobs[type].name
                                           : PD_STR_MOB_RAT);
}

int pd_mob_max_hp(uint8_t type) {
    return type < PD_MOB_TYPE_COUNT ? (int)kMobs[type].hp : 1;
}

uint8_t pd_mob_sprite(uint8_t type) {
    return type < PD_MOB_TYPE_COUNT ? kMobs[type].sprite
                                    : (uint8_t)PD_SPRITE_MOB(0, PD_MOB_FRAME_IDLE);
}

uint8_t pd_item_sprite(const pd_item_t *item) {
    const uint8_t tier = item->tier < 4 ? item->tier : 3;
    switch (item->kind) {
        case PD_ITEM_GOLD:
            return PD_SPRITE_ITEM_GOLD;
        case PD_ITEM_IRON_KEY:
            return PD_SPRITE_ITEM_IRON_KEY;
        case PD_ITEM_AMULET:
            return PD_SPRITE_ITEM_AMULET;
        case PD_ITEM_WAND_MAGIC:
            return PD_SPRITE_ITEM_WAND_MAGIC_MISSILE;
        case PD_ITEM_POTION_HEAL:
            return PD_SPRITE_ITEM_POTION_HEAL;
        case PD_ITEM_POTION_STRENGTH:
            return PD_SPRITE_ITEM_POTION_STRENGTH;
        case PD_ITEM_SCROLL_UPGRADE:
            return PD_SPRITE_ITEM_SCROLL_UPGRADE;
        case PD_ITEM_SCROLL_MAP:
            return PD_SPRITE_ITEM_SCROLL_MAP;
        case PD_ITEM_WEAPON:
            return (uint8_t)(PD_SPRITE_ITEM_DAGGER +
                             (tier == 1 ? 1 : (tier == 2 ? 2 : (tier >= 3 ? 3 : 0))));
        case PD_ITEM_ARMOR:
            return (uint8_t)(PD_SPRITE_ITEM_CLOTH + (tier ? tier - 1 : 0));
        default:
            return PD_SPRITE_ITEM_RATION;
    }
}

const char *pd_class_name(uint8_t cls) {
    return pd_str(kClasses[cls < 3 ? cls : 0].name);
}

int pd_hero_armor_value(const pd_game_t *game) {
    const pd_hero_t *hero = &game->hero;
    int value = 0;
    if (hero->armor >= 0 && hero->armor < game->bag_count) {
        const pd_item_t *armor = &game->bag[hero->armor];
        value = kArmorValue[armor->tier < 4 ? armor->tier : 3] + armor->level / 2;
    }
    return value;
}

uint8_t pd_hero_visual_tier(const pd_game_t *game) {
    const int slot = game->hero.armor;
    if (slot < 0 || slot >= game->bag_count ||
        game->bag[slot].kind != PD_ITEM_ARMOR) return 0;
    return game->bag[slot].tier < PD_SPRITE_HERO_TIERS ?
           game->bag[slot].tier : PD_SPRITE_HERO_TIERS - 1;
}

void pd_hero_damage_range(const pd_game_t *game, int *low, int *high) {
    const pd_hero_t *hero = &game->hero;
    int bonus = (hero->str > 10 ? (hero->str - 10) / 2 : 0);
    if (hero->weapon >= 0 && hero->weapon < game->bag_count) {
        const pd_item_t *weapon = &game->bag[hero->weapon];
        const int tier = weapon->tier < 4 ? weapon->tier : 3;
        *low = kWeaponMin[tier] + weapon->level + bonus;
        *high = kWeaponMax[tier] + weapon->level + bonus;
    } else {
        *low = 1 + bonus;
        *high = 3 + bonus;
    }
    if (*low < 1) *low = 1;
    if (*high < *low) *high = *low;
}

/* ---------------------------------------------------------------------- */
/* World queries                                                           */
/* ---------------------------------------------------------------------- */

static int mob_at(const pd_game_t *game, int x, int y) {
    for (int index = 0; index < PD_MOBS_MAX; ++index) {
        if (game->mobs[index].type == 0xFF || game->mobs[index].dying) continue;
        if (game->mobs[index].x == x && game->mobs[index].y == y) return index;
    }
    return -1;
}

static int ground_at(const pd_game_t *game, int x, int y) {
    for (int index = 0; index < game->ground_count; ++index)
        if (game->ground[index].used && game->ground[index].x == x &&
            game->ground[index].y == y)
            return index;
    return -1;
}

static int has_amulet(const pd_game_t *game) {
    for (int index = 0; index < game->bag_count; ++index)
        if (game->bag[index].kind == PD_ITEM_AMULET) return 1;
    return 0;
}

static int alive_mobs(const pd_game_t *game) {
    int count = 0;
    for (int index = 0; index < PD_MOBS_MAX; ++index)
        if (game->mobs[index].type != 0xFF && game->mobs[index].dying == 0)
            ++count;
    return count;
}

/* ---------------------------------------------------------------------- */
/* Spawning                                                                */
/* ---------------------------------------------------------------------- */

static pd_item_t make_item(uint8_t kind, uint8_t depth, uint32_t *rng) {
    pd_item_t item;
    item.kind = kind;
    item.tier = 1;
    item.level = 0;
    item.gold = 0;
    switch (kind) {
        case PD_ITEM_GOLD:
            item.gold = (uint16_t)pd_rng_range(rng, 6 + depth * 2, 18 + depth * 5);
            break;
        case PD_ITEM_WEAPON:
        case PD_ITEM_ARMOR:
            item.tier = (uint8_t)(1 + depth / 7 + (pd_rng_below(rng, 100) < 30 ? 1 : 0));
            if (item.tier > 4) item.tier = 4;
            break;
        case PD_ITEM_POTION_HEAL:
        case PD_ITEM_POTION_STRENGTH:
        case PD_ITEM_SCROLL_UPGRADE:
        case PD_ITEM_SCROLL_MAP:
            item.tier = (uint8_t)pd_rng_range(rng, 1, 3);
            break;
        case PD_ITEM_WAND_MAGIC:
            item.tier = 3;
            break;
        default:
            break;
    }
    return item;
}

static uint8_t pick_item_kind(uint32_t *rng, uint8_t depth) {
    const uint32_t roll = pd_rng_below(rng, 100);
    if (roll < 26) return PD_ITEM_POTION_HEAL;
    if (roll < 34) return PD_ITEM_POTION_STRENGTH;
    if (roll < 48) return PD_ITEM_SCROLL_UPGRADE;
    if (roll < 56) return PD_ITEM_SCROLL_MAP;
    if (roll < 65) return PD_ITEM_WEAPON;
    if (roll < 78) return PD_ITEM_ARMOR;
    if (roll < 86) return PD_ITEM_WAND_MAGIC;
    (void)depth;
    return PD_ITEM_FOOD;
}

static void place_ground_item(pd_game_t *game, const pd_item_t *item, int x,
                              int y) {
    if (game->ground_count >= PD_GROUND_MAX) return;
    game->ground[game->ground_count].used = 1;
    game->ground[game->ground_count].x = (uint8_t)x;
    game->ground[game->ground_count].y = (uint8_t)y;
    game->ground[game->ground_count].item = *item;
    ++game->ground_count;
}

static void drop_item_on_floor(pd_game_t *game, const pd_item_t *item, int x,
                               int y) {
    place_ground_item(game, item, x, y);
}

static void scatter_loot(pd_game_t *game, int x, int y, uint8_t depth,
                         uint32_t *rng, int count) {
    for (int index = 0; index < count; ++index) {
        int drop_x = x;
        int drop_y = y;
        if (!pd_level_nearest_open(&game->level, x, y, 3, &drop_x, &drop_y))
            continue;
        if (mob_at(game, drop_x, drop_y) >= 0) continue;
        {
            const uint8_t kind = pick_item_kind(rng, depth);
            pd_item_t item = make_item(kind, depth, rng);
            drop_item_on_floor(game, &item, drop_x, drop_y);
        }
    }
}

static void spawn_entities(pd_game_t *game, uint32_t seed) {
    uint32_t rng;
    int mob_target;
    int item_target;
    int chests;

    pd_rng_seed(&rng, pd_rng_mix(seed, (uint32_t)game->depth * 7919u + 17u));
    for (int index = 0; index < PD_MOBS_MAX; ++index) {
        game->mobs[index].type = 0xFF;
        game->mobs[index].dying = 0;
        game->mobs[index].attacking = 0;
        game->mobs[index].moving = 0;
    }
    game->ground_count = 0;
    game->change_count = 0;

    mob_target = 3 + game->depth / 3;
    if (mob_target > 9) mob_target = 9;
    for (int spawned = 0; spawned < mob_target; ++spawned) {
        for (int attempt = 0; attempt < 40; ++attempt) {
            const int room_index = pd_rng_range(&rng, 1, game->level.room_count - 1);
            if (game->level.room_count < 2) break;
            {
                const pd_room_t *room = &game->level.rooms[room_index];
                const int x = room->x + (int)pd_rng_below(&rng, room->w);
                const int y = room->y + (int)pd_rng_below(&rng, room->h);
                uint8_t type;
                if (!pd_tile_walkable(pd_tile_at(&game->level, x, y))) continue;
                if (mob_at(game, x, y) >= 0) continue;
                if (x == game->hero.x && y == game->hero.y) continue;
                {
                    uint8_t candidates[PD_MOB_TYPE_COUNT];
                    int count = 0;
                    for (int type = 0; type < PD_MOB_TYPE_COUNT; ++type) {
                        if (type == PD_MOB_DEMON) continue;
                        if (game->depth < kMobs[type].min_depth ||
                            game->depth > kMobs[type].max_depth)
                            continue;
                        candidates[count++] = (uint8_t)type;
                    }
                    if (count == 0) {
                        type = 0;
                    } else {
                        /* Deeper mobs join the pool as the hero descends. */
                        type = candidates[pd_rng_below(&rng, (uint32_t)count)];
                    }
                }
                {
                    int slot = -1;
                    for (int index = 0; index < PD_MOBS_MAX; ++index) {
                        if (game->mobs[index].type == 0xFF) {
                            slot = index;
                            break;
                        }
                    }
                    if (slot < 0) break;
                    game->mobs[slot].type = type;
                    game->mobs[slot].x = (uint8_t)x;
                    game->mobs[slot].y = (uint8_t)y;
                    game->mobs[slot].hp = (int16_t)kMobs[type].hp;
                    game->mobs[slot].awake = 0;
                    game->mobs[slot].wander = (uint8_t)pd_rng_range(&rng, 1, 4);
                    game->mobs[slot].dying = 0;
                    game->mobs[slot].attacking = 0;
                }
                break;
            }
        }
    }
    if (game->depth >= 25 && !has_amulet(game)) {
        int slot = -1;
        for (int index = 0; index < PD_MOBS_MAX; ++index) {
            if (game->mobs[index].type == 0xFF) {
                slot = index;
                break;
            }
        }
        if (slot >= 0) {
            int x = game->level.exit_x;
            int y = game->level.exit_y;
            if (!pd_level_nearest_open(&game->level, x, y, 3, &x, &y)) {
                x = game->level.entrance_x;
                y = game->level.entrance_y;
            }
            game->mobs[slot].type = PD_MOB_DEMON;
            game->mobs[slot].x = (uint8_t)x;
            game->mobs[slot].y = (uint8_t)y;
            game->mobs[slot].hp = (int16_t)kMobs[PD_MOB_DEMON].hp;
            game->mobs[slot].awake = 0;
            game->mobs[slot].wander = 1;
            game->mobs[slot].dying = 0;
            game->mobs[slot].attacking = 0;
        }
    }

    item_target = 2 + game->depth / 6;
    if (item_target > 5) item_target = 5;
    for (int index = 0; index < item_target; ++index) {
        for (int attempt = 0; attempt < 30; ++attempt) {
            const int room_index = pd_rng_range(&rng, 0, game->level.room_count - 1);
            const pd_room_t *room = &game->level.rooms[room_index];
            const int x = room->x + (int)pd_rng_below(&rng, room->w);
            const int y = room->y + (int)pd_rng_below(&rng, room->h);
            if (!pd_tile_walkable(pd_tile_at(&game->level, x, y))) continue;
            if (ground_at(game, x, y) >= 0) continue;
            {
                const uint8_t kind = pick_item_kind(&rng, game->depth);
                pd_item_t item = make_item(kind, game->depth, &rng);
                drop_item_on_floor(game, &item, x, y);
            }
            break;
        }
    }
    /* A healing potion is always somewhere on the floor. */
    for (int attempt = 0; attempt < 30; ++attempt) {
        const int room_index = pd_rng_range(&rng, 0, game->level.room_count - 1);
        const pd_room_t *room = &game->level.rooms[room_index];
        const int x = room->x + (int)pd_rng_below(&rng, room->w);
        const int y = room->y + (int)pd_rng_below(&rng, room->h);
        if (!pd_tile_walkable(pd_tile_at(&game->level, x, y))) continue;
        if (ground_at(game, x, y) >= 0) continue;
        {
            pd_item_t item = make_item(PD_ITEM_POTION_HEAL, game->depth, &rng);
            drop_item_on_floor(game, &item, x, y);
        }
        break;
    }
    /* Gold piles in a few rooms. */
    for (int index = 0; index < 3; ++index) {
        const int room_index = pd_rng_range(&rng, 0, game->level.room_count - 1);
        const pd_room_t *room = &game->level.rooms[room_index];
        const int x = room->x + (int)pd_rng_below(&rng, room->w);
        const int y = room->y + (int)pd_rng_below(&rng, room->h);
        if (!pd_tile_walkable(pd_tile_at(&game->level, x, y))) continue;
        if (ground_at(game, x, y) >= 0) continue;
        {
            pd_item_t item = make_item(PD_ITEM_GOLD, game->depth, &rng);
            drop_item_on_floor(game, &item, x, y);
        }
    }
    /* One chest per floor, placed against a room wall. */
    chests = game->level.room_count > 1 ? 1 : 0;
    for (int index = 0; index < chests; ++index) {
        for (int attempt = 0; attempt < 30; ++attempt) {
            const int room_index = pd_rng_range(&rng, 0, game->level.room_count - 1);
            const pd_room_t *room = &game->level.rooms[room_index];
            const int x = room->x + (int)pd_rng_below(&rng, room->w);
            const int y = room->y + (int)pd_rng_below(&rng, room->h);
            const uint8_t tile = pd_tile_at(&game->level, x, y);
            if (tile != game->level.floor_tile) continue;
            if (ground_at(game, x, y) >= 0) continue;
            if (mob_at(game, x, y) >= 0) continue;
            if (x == game->hero.x && y == game->hero.y) continue;
            if (pd_tile_stairs_down(game->level.tiles[y * PD_MAP_W + x]))
                continue;
            game->level.tiles[y * PD_MAP_W + x] =
                PD_TILE(game->level.theme, PD_TILEK_CHEST);
            record_change(game, x, y, game->level.tiles[y * PD_MAP_W + x]);
            break;
        }
    }
}

/* ---------------------------------------------------------------------- */
/* Run setup                                                               */
/* ---------------------------------------------------------------------- */

static void hero_init(pd_game_t *game, uint8_t cls) {
    const pd_class_def_t *def = &kClasses[cls < 3 ? cls : 0];
    if (game->roll_rng == 0)
        game->roll_rng = pd_rng_mix(game->run_seed, UINT32_C(0x5bf03635));
    pd_hero_t *hero = &game->hero;
    hero->cls = cls < 3 ? cls : 0;
    hero->max_hp = (int16_t)def->hp;
    hero->hp = hero->max_hp;
    hero->str = def->str;
    hero->level = 1;
    hero->xp = 0;
    hero->gold = 0;
    hero->keys = 0;
    hero->poison = 0;
    hero->hunger = PD_HUNGER_MAX;
    hero->weapon = -1;
    hero->armor = -1;
    game->bag_count = 0;
    game->depth = 1;
    game->turn = 0;
    game->kills = 0;
    game->change_count = 0;
    game->message_total = 0;
    game->walk_active = 0;
    game->hero_moving = 0;
    game->sound_count = 0;
    game->bag_selected = -1;
    game->wand_slot = -1;
    for (int index = 0; index < PD_EFFECTS_MAX; ++index) game->effects[index].ttl = 0;
    if (def->weapon_tier > 0) {
        pd_item_t weapon;
        weapon.kind = PD_ITEM_WEAPON;
        weapon.tier = def->weapon_tier;
        weapon.level = 0;
        weapon.gold = 0;
        game->bag[game->bag_count] = weapon;
        hero->weapon = (int8_t)game->bag_count;
        ++game->bag_count;
    }
    if (def->armor_tier > 0) {
        pd_item_t armor;
        armor.kind = PD_ITEM_ARMOR;
        armor.tier = def->armor_tier;
        armor.level = 0;
        armor.gold = 0;
        game->bag[game->bag_count] = armor;
        hero->armor = (int8_t)game->bag_count;
        ++game->bag_count;
    }
    for (int index = 0; index < def->potions; ++index) {
        pd_item_t potion;
        potion.kind = PD_ITEM_POTION_HEAL;
        potion.tier = 1;
        potion.level = 0;
        potion.gold = 0;
        game->bag[game->bag_count++] = potion;
    }
    {
        pd_item_t food;
        food.kind = PD_ITEM_FOOD;
        food.tier = 1;
        food.level = 0;
        food.gold = 0;
        game->bag[game->bag_count++] = food;
    }
}

static void place_special_tiles(pd_game_t *game) {
    int door_count = 0;
    game->lock_x = game->lock_y = 255;
    game->shop_x = game->shop_y = 255;
    if (game->generation == 0) return;
    if (game->depth >= 2 && game->depth < 25) {
        uint32_t rng = pd_rng_mix(game->run_seed, game->depth * 713u);
        for (int y = 1; y < PD_MAP_H - 1; ++y)
            for (int x = 1; x < PD_MAP_W - 1; ++x)
                if (PD_TILE_KIND(pd_tile_at(&game->level, x, y)) == PD_TILEK_DOOR &&
                    (int)pd_rng_below(&rng, (uint32_t)++door_count) == 0) {
                    game->lock_x = (uint8_t)x;
                    game->lock_y = (uint8_t)y;
                }
    }
    if (game->depth > 1 && game->depth % 5 == 1) {
        const int entrance_x = game->level.entrance_x;
        const int entrance_y = game->level.entrance_y;
        for (int radius = 2; radius <= 5 && game->shop_x == 255; ++radius)
            for (int y = entrance_y - radius; y <= entrance_y + radius &&
                 game->shop_x == 255; ++y)
                for (int x = entrance_x - radius; x <= entrance_x + radius;
                     ++x) {
                    if (!pd_in_bounds(x, y) ||
                        PD_TILE_KIND(pd_tile_at(&game->level, x, y)) !=
                            PD_TILEK_FLOOR ||
                        !pd_tile_walkable(pd_tile_at(&game->level, x - 1, y)) ||
                        !pd_tile_walkable(pd_tile_at(&game->level, x + 1, y)) ||
                        !pd_tile_walkable(pd_tile_at(&game->level, x, y - 1)) ||
                        !pd_tile_walkable(pd_tile_at(&game->level, x, y + 1)))
                        continue;
                    game->shop_x = (uint8_t)x;
                    game->shop_y = (uint8_t)y;
                    game->level.tiles[y * PD_MAP_W + x] =
                        PD_TILE(game->level.theme, PD_TILEK_ALCHEMY);
                    break;
                }
    }
    if (game->generation >= 3 && game->level.room_count > 2 &&
        game->depth % 5 != 0) {
        uint32_t rng = pd_rng_mix(game->run_seed,
                                  game->depth * 1723u + 91u);
        const int room_index = 1 + (int)pd_rng_below(
            &rng, (uint32_t)(game->level.room_count - 1));
        const pd_room_t *room = &game->level.rooms[room_index];
        const int style = game->level.theme <= 1 ?
                          (int)pd_rng_below(&rng, 2) :
                          game->level.theme == 2 ? 1 : 2;
        for (int y = room->y + 1; y < room->y + room->h - 1; ++y)
            for (int x = room->x + 1; x < room->x + room->w - 1; ++x) {
                const int at = y * PD_MAP_W + x;
                const uint8_t kind = PD_TILE_KIND(game->level.tiles[at]);
                if (kind == PD_TILEK_VOID || pd_tile_wall(game->level.tiles[at]) ||
                    kind == PD_TILEK_DOOR || kind == PD_TILEK_DOOR_OPEN ||
                    kind == PD_TILEK_STAIRS_UP || kind == PD_TILEK_STAIRS_DOWN ||
                    kind == PD_TILEK_CHEST || kind == PD_TILEK_ALCHEMY ||
                    kind == PD_TILEK_STATUE) continue;
                const uint32_t roll = pd_rng_below(&rng, 100);
                const uint8_t feature = style == 0 ?
                    (roll < 60 ? PD_TILEK_HIGH_GRASS : PD_TILEK_GRASS) :
                    style == 1 ?
                    (roll < 78 ? PD_TILEK_WATER_A : PD_TILEK_GRASS) :
                    (roll < 58 ? PD_TILEK_EMBERS_A : PD_TILEK_DECO_ALT);
                game->level.tiles[at] = PD_TILE(game->level.theme, feature);
            }
    }
    if (game->generation >= 2) {
        uint32_t rng = pd_rng_mix(game->run_seed,
                                  game->depth * 1031u + 79u);
        const int trap_count = game->depth == 1 ? 1 :
                               2 + game->depth / 8;
        for (int placed = 0; placed < trap_count; ++placed) {
            for (int attempt = 0; attempt < 80; ++attempt) {
                const int x = pd_rng_range(&rng, 2, PD_MAP_W - 3);
                const int y = pd_rng_range(&rng, 2, PD_MAP_H - 3);
                const int entrance_dx = x - game->level.entrance_x;
                const int entrance_dy = y - game->level.entrance_y;
                const int exit_dx = x - game->level.exit_x;
                const int exit_dy = y - game->level.exit_y;
                if (PD_TILE_KIND(pd_tile_at(&game->level, x, y)) !=
                        PD_TILEK_FLOOR ||
                    (entrance_dx >= -2 && entrance_dx <= 2 &&
                     entrance_dy >= -2 && entrance_dy <= 2) ||
                    (exit_dx >= -1 && exit_dx <= 1 &&
                     exit_dy >= -1 && exit_dy <= 1)) continue;
                game->level.tiles[y * PD_MAP_W + x] =
                    PD_TILE(game->level.theme, PD_TILEK_TRAP);
                break;
            }
        }
    }
}

static void place_floor_key(pd_game_t *game) {
    if (game->lock_x == 255) return;
    if (game->ground_count >= PD_GROUND_MAX) {
        game->hero.keys = 1;
        return;
    }
    const int start_x = game->level.entrance_x;
    const int start_y = game->level.entrance_y;
    const pd_item_t key = {PD_ITEM_IRON_KEY, 0, 0, 0};
    const int offsets[4][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
    for (int index = 0; index < 4; ++index) {
        const int x = start_x + offsets[index][0];
        const int y = start_y + offsets[index][1];
        if (!pd_in_bounds(x, y) ||
            !pd_tile_walkable(pd_tile_at(&game->level, x, y)) ||
            PD_TILE_KIND(pd_tile_at(&game->level, x, y)) == PD_TILEK_DOOR ||
            ground_at(game, x, y) >= 0 || mob_at(game, x, y) >= 0)
            continue;
        place_ground_item(game, &key, x, y);
        return;
    }
    place_ground_item(game, &key, start_x, start_y);
}

static void hero_pickup(pd_game_t *game);

void pd_game_enter_depth(pd_game_t *game, uint8_t depth) {
    const int ascending = depth < game->depth;
    if (game->generation > 0 && game->generation < 3 &&
        depth != game->depth)
        game->generation = 3;
    if (depth < 1) depth = 1;
    if (depth > 25) depth = 25;
    game->depth = depth;
    if (depth > game->deepest) game->deepest = depth;
    if (game->generation == 0)
        pd_level_generate_legacy(&game->level, game->run_seed, depth);
    else
        pd_level_generate(&game->level, game->run_seed, depth);
    place_special_tiles(game);
    game->hero.keys = 0;
    game->change_count = 0;
    game->hero.x = ascending ? game->level.exit_x : game->level.entrance_x;
    game->hero.y = ascending ? game->level.exit_y : game->level.entrance_y;
    game->hero_from_x = game->hero.x;
    game->hero_from_y = game->hero.y;
    game->hero_moving = 0;
    game->hero.facing = 0;
    spawn_entities(game, pd_rng_mix(game->run_seed, depth * 131u + 5u));
    place_floor_key(game);
    hero_pickup(game);
    pd_level_update_fov(&game->level, game->hero.x, game->hero.y);
    game->walk_active = 0;
    game->wand_slot = -1;
    message_num(game, PD_MSG_INFO,
                ascending ? PD_STR_CLIMB_UP : PD_STR_CLIMB_DOWN, depth);
    sound_add(game, PD_SOUND_DESCEND);
    if (depth >= 25 && !has_amulet(game)) {
        message_simple(game, PD_MSG_BAD, PD_STR_BOSS_STIRS);
    }
}

void pd_game_reset(pd_game_t *game, uint32_t seed) {
    game->run_seed = seed;
    game->roll_rng = pd_rng_mix(seed, UINT32_C(0x5bf03635));
    game->phase = PD_PHASE_TITLE;
    game->generation = 3;
    game->class_choice = 0;
    game->hero.x = 0;
    game->hero.y = 0;
    game->message_total = 0;
    game->bag_count = 0;
    game->ground_count = 0;
    game->change_count = 0;
    game->walk_active = 0;
    game->bag_selected = -1;
    game->wand_slot = -1;
    game->hero.keys = 0;
    game->hero.poison = 0;
    game->potion_hint = 0;
    game->kills = 0;
    game->deepest = 1;
    game->depth = 1;
    game->turn = 0;
    for (int index = 0; index < PD_EFFECTS_MAX; ++index) game->effects[index].ttl = 0;
}

void pd_game_start_run(pd_game_t *game, uint8_t class_choice) {
    hero_init(game, class_choice);
    game->phase = PD_PHASE_PLAY;
    message_simple(game, PD_MSG_INFO, PD_STR_ENTER_DUNGEON);
    pd_game_enter_depth(game, 1);
}

/* ---------------------------------------------------------------------- */
/* Combat                                                                  */
/* ---------------------------------------------------------------------- */

static void gain_xp(pd_game_t *game, int amount);
static void damage_hero(pd_game_t *game, int amount, const char *source) {
    const int starvation = source == NULL;
    int taken = amount - (starvation ? 0 : pd_hero_armor_value(game));
    if (taken < 1) taken = 1;
    game->hero.hp = (int16_t)(game->hero.hp - taken);
    effect_add(game, PD_EFFECT_DAMAGE, game->hero.x, game->hero.y, taken);
    if (starvation)
        message_num(game, PD_MSG_BAD, PD_STR_STARVATION_DAMAGE, taken);
    else {
        sound_add(game, PD_SOUND_HIT_STRONG);
        message_text_num(game, PD_MSG_BAD, PD_STR_MOB_HITS_YOU, source, taken);
    }
    if (game->hero.hp <= 0) {
        game->hero.hp = 0;
        game->phase = PD_PHASE_DEAD;
        game->walk_active = 0;
        message_simple(game, PD_MSG_BAD,
                       starvation ? PD_STR_STARVED : PD_STR_YOU_DIE);
    }
}

static void poison_tick(pd_game_t *game) {
    if (game->hero.poison == 0) return;
    --game->hero.poison;
    --game->hero.hp;
    effect_add(game, PD_EFFECT_DAMAGE, game->hero.x, game->hero.y, 1);
    effect_add(game, PD_EFFECT_POISON, game->hero.x, game->hero.y, 0);
    message_simple(game, PD_MSG_BAD, PD_STR_POISON_DAMAGE);
    if (game->hero.hp <= 0) {
        game->hero.hp = 0;
        game->phase = PD_PHASE_DEAD;
        game->walk_active = 0;
        message_simple(game, PD_MSG_BAD, PD_STR_POISON_DEATH);
    } else if (game->hero.poison == 0) {
        message_simple(game, PD_MSG_GOOD, PD_STR_POISON_FADE);
    }
}

static void damage_mob(pd_game_t *game, int mob_index, int amount) {
    pd_mob_t *mob = &game->mobs[mob_index];
    pd_text_t line;
    if (amount < 1) amount = 1;
    mob->hp = (int16_t)(mob->hp - amount);
    mob->awake |= PD_MOB_AWAKE;
    sound_add(game, PD_SOUND_HIT);
    effect_add(game, PD_EFFECT_DAMAGE, mob->x, mob->y, amount);
    effect_add(game, PD_EFFECT_BLOOD, mob->x, mob->y, 0);
    message_text_num(game, PD_MSG_GOOD, PD_STR_HIT_MOB,
                     pd_mob_name(mob->type), amount);
    if (mob->hp > 0) {
        if (mob->type == PD_MOB_SWARM && mob->hp >= 4 &&
            !(mob->awake & PD_MOB_SPLIT)) {
            static const int8_t directions[4][2] = {
                {1, 0}, {0, 1}, {-1, 0}, {0, -1}};
            int slot = -1;
            for (int index = 0; index < PD_MOBS_MAX; ++index)
                if (game->mobs[index].type == 0xFF) {
                    slot = index;
                    break;
                }
            if (slot >= 0) {
                const int first = (int)pd_rng_below(&game->roll_rng, 4);
                for (int offset = 0; offset < 4; ++offset) {
                    const int direction = (first + offset) & 3;
                    const int x = mob->x + directions[direction][0];
                    const int y = mob->y + directions[direction][1];
                    if (!pd_in_bounds(x, y) ||
                        (x == game->hero.x && y == game->hero.y) ||
                        !pd_tile_walkable(pd_tile_at(&game->level, x, y)) ||
                        mob_at(game, x, y) >= 0) continue;
                    pd_mob_t *clone = &game->mobs[slot];
                    clone->type = PD_MOB_SWARM;
                    clone->x = clone->from_x = (uint8_t)x;
                    clone->y = clone->from_y = (uint8_t)y;
                    clone->hp = mob->hp / 2;
                    clone->awake = PD_MOB_AWAKE | PD_MOB_SPLIT |
                                   PD_MOB_NO_XP | PD_MOB_SPLIT_WAIT;
                    clone->wander = 0;
                    clone->dying = clone->attacking = clone->moving = 0;
                    mob->hp -= clone->hp;
                    mob->awake |= PD_MOB_SPLIT;
                    effect_add(game, PD_EFFECT_SWARM, x, y, 0);
                    message_simple(game, PD_MSG_WARN, PD_STR_SWARM_SPLITS);
                    break;
                }
            }
        }
        return;
    }
    {
        const uint8_t type = mob->type;
        const int xp = mob->awake & PD_MOB_NO_XP ? 0 : kMobs[type].xp;
        mob->hp = 0;
        mob->dying = 22;
        sound_add(game, PD_SOUND_DEATH);
        ++game->kills;
        effect_add(game, PD_EFFECT_BLOOD, mob->x, mob->y, 0);
        message_text(game, PD_MSG_GOOD, PD_STR_MOB_DIES,
                     pd_str(kMobs[type].name));
        gain_xp(game, xp);
        if (type == PD_MOB_DEMON && game->depth >= 25) {
            const pd_item_t amulet = {PD_ITEM_AMULET, 0, 0, 0};
            drop_item_on_floor(game, &amulet, mob->x, mob->y);
            game->walk_active = 0;
            message_simple(game, PD_MSG_GOOD, PD_STR_AMULET_DROPS);
        }
    }
}

static int xp_needed(const pd_hero_t *hero) {
    return 6 + hero->level * 5;
}

static void gain_xp(pd_game_t *game, int amount) {
    pd_hero_t *hero = &game->hero;
    hero->xp = (uint8_t)(hero->xp + amount);
    while (hero->xp >= xp_needed(hero)) {
        hero->xp = (uint8_t)(hero->xp - xp_needed(hero));
        ++hero->level;
        hero->max_hp = (int16_t)(hero->max_hp + 5);
        hero->hp = hero->max_hp;
        if ((hero->level & 1) == 0) ++hero->str;
        {
            message_num(game, PD_MSG_GOOD, PD_STR_LEVEL_UP, hero->level);
            sound_add(game, PD_SOUND_LEVELUP);
        }
    }
}

static void hero_attack(pd_game_t *game, int mob_index) {
    pd_mob_t *mob = &game->mobs[mob_index];
    const pd_mob_def_t *def = &kMobs[mob->type];
    int low = 1;
    int high = 3;
    int hit_chance = 76 + game->hero.level - def->evasion;
    pd_text_t line;

    if (hit_chance < 25) hit_chance = 25;
    if (hit_chance > 95) hit_chance = 95;
    if ((int)pd_rng_below(&game->roll_rng, 100u) >= hit_chance) {
        message_text(game, PD_MSG_WARN, PD_STR_MISS_MOB,
                     pd_str(def->name));
        sound_add(game, PD_SOUND_MISS);
        return;
    }
    pd_hero_damage_range(game, &low, &high);
    damage_mob(game, mob_index,
               pd_rng_range(&game->roll_rng, low, high) - (int)def->armor);
}

static void mob_attack_hero(pd_game_t *game, int mob_index) {
    const pd_mob_t *mob = &game->mobs[mob_index];
    const pd_mob_def_t *def = &kMobs[mob->type];
    const pd_class_def_t *hero_class = &kClasses[game->hero.cls];
    int hit_chance = (int)def->accuracy - (int)hero_class->dodge -
                     game->hero.level / 2;
    pd_text_t line;

    if (hit_chance < 20) hit_chance = 20;
    if (hit_chance > 92) hit_chance = 92;
    if ((int)pd_rng_below(&game->roll_rng, 100u) >= hit_chance) {
        message_text(game, PD_MSG_NEUTRAL, PD_STR_MOB_MISSES,
                     pd_str(def->name));
        return;
    }
    damage_hero(game, pd_rng_range(&game->roll_rng, def->dmg_min, def->dmg_max),
                pd_str(def->name));
    if (game->phase == PD_PHASE_PLAY && mob->type == PD_MOB_SPINNER &&
        pd_rng_below(&game->roll_rng, 100) < 35) {
        if (game->hero.poison < 5) game->hero.poison = 5;
        message_simple(game, PD_MSG_BAD, PD_STR_POISONED);
    }
}

/* ---------------------------------------------------------------------- */
/* Turns                                                                   */
/* ---------------------------------------------------------------------- */

static int trample_grass(pd_game_t *game, int x, int y) {
    if (PD_TILE_KIND(pd_tile_at(&game->level, x, y)) != PD_TILEK_HIGH_GRASS)
        return 0;
    game->level.tiles[y * PD_MAP_W + x] =
        PD_TILE(game->level.theme, PD_TILEK_GRASS);
    record_change(game, x, y, game->level.tiles[y * PD_MAP_W + x]);
    effect_add(game, PD_EFFECT_LEAF, x, y, 0);
    return 1;
}

static int mob_step(pd_game_t *game, pd_mob_t *mob, int dx, int dy) {
    const int x = mob->x + dx;
    const int y = mob->y + dy;
    if (!pd_in_bounds(x, y)) return 0;
    if (x == game->hero.x && y == game->hero.y) return 0;
    if (!pd_tile_walkable(pd_tile_at(&game->level, x, y))) return 0;
    if (mob_at(game, x, y) >= 0) return 0;
    mob->from_x = mob->x;
    mob->from_y = mob->y;
    mob->moving = 8;
    mob->x = (uint8_t)x;
    mob->y = (uint8_t)y;
    (void)trample_grass(game, x, y);
    return 1;
}

static void mobs_turn(pd_game_t *game) {
    for (int index = 0; index < PD_MOBS_MAX; ++index) {
        pd_mob_t *mob = &game->mobs[index];
        const pd_mob_def_t *def;
        int dx;
        int dy;
        int distance;
        if (mob->type == 0xFF || mob->dying) continue;
        if (mob->awake & PD_MOB_SPLIT_WAIT) {
            mob->awake &= (uint8_t)~PD_MOB_SPLIT_WAIT;
            continue;
        }
        def = &kMobs[mob->type];
        dx = game->hero.x - mob->x;
        dy = game->hero.y - mob->y;
        if (dx < 0) dx = -dx;
        if (dy < 0) dy = -dy;
        distance = dx > dy ? dx : dy;
        {
            const int in_sight = game->level.visible[mob->y * PD_MAP_W + mob->x];
            if (!(mob->awake & PD_MOB_AWAKE)) {
                if (in_sight && distance <= 8) {
                    mob->awake |= PD_MOB_AWAKE;
                    message_text(game, PD_MSG_WARN, PD_STR_MOB_NOTICES,
                                 pd_str(def->name));
                } else {
                    if (mob->wander > 0) --mob->wander;
                    if (mob->wander == 0) {
                        const int direction = (int)pd_rng_below(&game->roll_rng, 5u);
                        static const int8_t kIdle[4][2] = {
                            {0, -1}, {0, 1}, {-1, 0}, {1, 0}};
                        if (direction < 4)
                            (void)mob_step(game, mob, kIdle[direction][0],
                                           kIdle[direction][1]);
                        mob->wander = (uint8_t)pd_rng_range(&game->roll_rng, 2, 6);
                    }
                    continue;
                }
            }
            if (distance <= 1) {
                mob->attacking = 12;
                mob_attack_hero(game, index);
                if (game->phase != PD_PHASE_PLAY) return;
                continue;
            }
            if (mob->type == PD_MOB_DM100 && in_sight && distance <= 4) {
                mob->attacking = 12;
                effect_add(game, PD_EFFECT_LIGHTNING, game->hero.x,
                           game->hero.y, (mob->x << 8) | mob->y);
                sound_add(game, PD_SOUND_LIGHTNING);
                if (pd_rng_below(&game->roll_rng, 100) < 76)
                    damage_hero(game,
                                pd_rng_range(&game->roll_rng, 3, 10) +
                                pd_hero_armor_value(game),
                                pd_str(def->name));
                if (game->phase != PD_PHASE_PLAY) return;
                continue;
            }
            if (!in_sight && distance > 9) {
                mob->awake &= (uint8_t)~PD_MOB_AWAKE;
                continue;
            }
        }
        {
            const int step_x = (game->hero.x > mob->x) - (game->hero.x < mob->x);
            const int step_y = (game->hero.y > mob->y) - (game->hero.y < mob->y);
            if (mob_step(game, mob, step_x, step_y)) continue;
            if (mob_step(game, mob, step_x, 0)) continue;
            if (mob_step(game, mob, 0, step_y)) continue;
        }
    }
}

static void hunger_tick(pd_game_t *game) {
    if (game->hero.hunger > 0) {
        --game->hero.hunger;
        if (game->hero.hunger == PD_HUNGER_WARN) {
            message_simple(game, PD_MSG_WARN, PD_STR_HUNGRY);
            sound_add(game, PD_SOUND_HUNGRY);
        } else if (game->hero.hunger == 0) {
            message_simple(game, PD_MSG_BAD, PD_STR_STARVING);
        }
        return;
    }
    if ((game->turn % 6u) == 0) {
        damage_hero(game, 1, NULL);
    }
}

static void open_chest(pd_game_t *game, int x, int y) {
    game->level.tiles[y * PD_MAP_W + x] =
        PD_TILE(game->level.theme, PD_TILEK_FLOOR_ALT);
    record_change(game, x, y, game->level.tiles[y * PD_MAP_W + x]);
    message_simple(game, PD_MSG_GOOD, PD_STR_CHEST_OPEN);
    sound_add(game, PD_SOUND_UNLOCK);
    scatter_loot(game, x, y, game->depth, &game->roll_rng, 3);
}

static void trigger_trap(pd_game_t *game, int x, int y) {
    const int variant = pd_trap_variant(game->run_seed, game->depth, x, y);
    const int damage = variant == 0 ? 3 + game->depth / 3 :
                                     4 + game->depth / 2;
    game->level.known[y * PD_MAP_W + x] = 1;
    game->level.tiles[y * PD_MAP_W + x] =
        PD_TILE(game->level.theme, PD_TILEK_FLOOR_ALT);
    record_change(game, x, y, game->level.tiles[y * PD_MAP_W + x]);
    message_num(game, PD_MSG_BAD, variant == 0 ? PD_STR_TRAP_DART_HIT :
                PD_STR_TRAP_BLAST_HIT, damage);
    sound_add(game, PD_SOUND_TRAP);
    effect_add(game, variant == 0 ? PD_EFFECT_TRAP_DART :
                                       PD_EFFECT_TRAP_BLAST, x, y, 0);
    damage_hero(game, damage + pd_hero_armor_value(game), "The trap");
    if (variant == 0 && game->phase == PD_PHASE_PLAY) {
        if (game->hero.poison < 4) game->hero.poison = 4;
        message_simple(game, PD_MSG_BAD, PD_STR_POISONED);
        effect_add(game, PD_EFFECT_POISON, x, y, 0);
    } else if (variant == 1) {
        sound_add(game, PD_SOUND_BLAST);
        for (int index = 0; index < PD_MOBS_MAX; ++index) {
            pd_mob_t *mob = &game->mobs[index];
            if (mob->type == 0xFF || mob->dying ||
                (mob->awake & PD_MOB_SPLIT_WAIT)) continue;
            const int dx = mob->x - x;
            const int dy = mob->y - y;
            if (dx >= -1 && dx <= 1 && dy >= -1 && dy <= 1)
                damage_mob(game, index, damage);
        }
    }
}

static void hero_pickup(pd_game_t *game) {
    for (int index = 0; index < game->ground_count;) {
        pd_ground_t *entry = &game->ground[index];
        if (!entry->used || entry->x != game->hero.x ||
            entry->y != game->hero.y) {
            ++index;
            continue;
        }
        if (entry->item.kind == PD_ITEM_GOLD) {
            pd_text_t line;
            game->hero.gold =
                (uint16_t)(game->hero.gold + entry->item.gold);
            message_num(game, PD_MSG_GOOD, PD_STR_PICK_GOLD,
                        entry->item.gold);
            sound_add(game, PD_SOUND_GOLD);
        } else if (entry->item.kind == PD_ITEM_IRON_KEY) {
            if (game->hero.keys < 255) ++game->hero.keys;
            message_simple(game, PD_MSG_GOOD, PD_STR_PICK_KEY);
            sound_add(game, PD_SOUND_ITEM);
        } else if (game->bag_count >= PD_BAG_MAX) {
            message_simple(game, PD_MSG_WARN, PD_STR_PACK_FULL);
            ++index;
            continue;
        } else {
            pd_item_t item = entry->item;
            message_item(game, PD_MSG_GOOD, PD_STR_PICK_ITEM, &item);
            sound_add(game, PD_SOUND_ITEM);
            game->bag[game->bag_count++] = item;
            if (item.kind == PD_ITEM_AMULET) {
                game->phase = PD_PHASE_AMULET;
                game->walk_active = 0;
                message_simple(game, PD_MSG_GOOD, PD_STR_AMULET_FOUND);
            }
        }
        entry->used = 0;
        for (int move = index; move + 1 < game->ground_count; ++move)
            game->ground[move] = game->ground[move + 1];
        --game->ground_count;
    }
}

static void end_turn(pd_game_t *game) {
    if (game->phase != PD_PHASE_PLAY) return;
    ++game->turn;
    mobs_turn(game);
    if (game->phase != PD_PHASE_PLAY) return;
    poison_tick(game);
    if (game->phase != PD_PHASE_PLAY) return;
    hunger_tick(game);
    if (game->phase != PD_PHASE_PLAY) return;
    if ((game->turn % 12u) == 0 && game->hero.hp < game->hero.max_hp &&
        game->hero.hunger > 0) {
        ++game->hero.hp;
    }
    if ((game->turn % 12u) == 0) {
        for (int index = 0; index < game->bag_count; ++index) {
            pd_item_t *item = &game->bag[index];
            if (item->kind == PD_ITEM_WAND_MAGIC &&
                item->tier < 3 + item->level)
                ++item->tier;
        }
    }
    if (game->potion_hint > 0) --game->potion_hint;
    pd_level_update_fov(&game->level, game->hero.x, game->hero.y);
    const int radius = game->hero.cls == 1 ? 2 : 1;
    for (int dy = -radius; dy <= radius; ++dy)
        for (int dx = -radius; dx <= radius; ++dx) {
            const int x = game->hero.x + dx;
            const int y = game->hero.y + dy;
            if (!pd_in_bounds(x, y) ||
                !game->level.visible[y * PD_MAP_W + x] ||
                !pd_tile_trap(pd_tile_at(&game->level, x, y)) ||
                game->level.known[y * PD_MAP_W + x]) continue;
            if (pd_rng_below(&game->roll_rng, 100) >=
                (uint32_t)(40 - game->depth * 10 / 25)) continue;
            game->level.known[y * PD_MAP_W + x] = 1;
            message_simple(game, PD_MSG_GOOD, PD_STR_SEARCH_TRAP);
            effect_add(game, PD_EFFECT_SPARK, x, y, 0);
        }
}

static void hero_act_move(pd_game_t *game, int dx, int dy, int from_walk) {
    pd_hero_t *hero = &game->hero;
    int nx;
    int ny;
    uint8_t tile;

    if (game->phase != PD_PHASE_PLAY) return;
    if (dx < 0) {
        hero->facing = 2;
    } else if (dx > 0) {
        hero->facing = 3;
    } else if (dy < 0) {
        hero->facing = 1;
    } else if (dy > 0) {
        hero->facing = 0;
    }
    nx = hero->x + dx;
    ny = hero->y + dy;
    if (!pd_in_bounds(nx, ny)) return;
    {
        const int mob = mob_at(game, nx, ny);
        if (mob >= 0) {
            hero_attack(game, mob);
            end_turn(game);
            return;
        }
    }
    tile = pd_tile_at(&game->level, nx, ny);
    if (nx == game->shop_x && ny == game->shop_y) {
        game->phase = PD_PHASE_SHOP;
        game->walk_active = 0;
        return;
    }
    if (PD_TILE_KIND(tile) == PD_TILEK_CHEST) {
        open_chest(game, nx, ny);
        end_turn(game);
        return;
    }
    if (PD_TILE_KIND(tile) == PD_TILEK_DOOR) {
        if (nx == game->lock_x && ny == game->lock_y) {
            if (!hero->keys) {
                message_simple(game, PD_MSG_WARN, PD_STR_NEED_KEY);
                game->walk_active = 0;
                return;
            }
            --hero->keys;
            message_simple(game, PD_MSG_GOOD, PD_STR_KEY_UNLOCK);
            sound_add(game, PD_SOUND_UNLOCK);
        }
        game->level.tiles[ny * PD_MAP_W + nx] =
            PD_TILE(game->level.theme, PD_TILEK_DOOR_OPEN);
        record_change(game, nx, ny, game->level.tiles[ny * PD_MAP_W + nx]);
        message_simple(game, PD_MSG_INFO, PD_STR_DOOR_OPEN);
        sound_add(game, PD_SOUND_DOOR);
    } else if (!pd_tile_walkable(tile)) {
        if (from_walk) {
            game->walk_active = 0;
        } else {
            message_simple(game, PD_MSG_INFO, PD_STR_WALL_BLOCKS);
        }
        return;
    }
    game->hero_from_x = hero->x;
    game->hero_from_y = hero->y;
    hero->x = (uint8_t)nx;
    hero->y = (uint8_t)ny;
    game->hero_moving = 12;
    if (trample_grass(game, nx, ny)) {
        sound_add(game, PD_SOUND_TRAMPLE);
    } else if (PD_TILE_KIND(tile) == PD_TILEK_GRASS) {
        sound_add(game, PD_SOUND_GRASS);
    } else if (pd_tile_water(tile)) {
        sound_add(game, PD_SOUND_WATER);
    } else {
        sound_add(game, PD_SOUND_STEP);
    }
    hero_pickup(game);
    if (pd_tile_trap(pd_tile_at(&game->level, hero->x, hero->y)))
        trigger_trap(game, hero->x, hero->y);
    end_turn(game);
}

void pd_game_hero_step(pd_game_t *game, int dx, int dy) {
    game->walk_active = 0;
    game->wand_slot = -1;
    if (dx == 0 && dy == 0) {
        pd_game_hero_wait(game);
        return;
    }
    hero_act_move(game, dx, dy, 0);
}

void pd_game_hero_wait(pd_game_t *game) {
    if (game->phase != PD_PHASE_PLAY) return;
    game->walk_active = 0;
    game->wand_slot = -1;
    message_simple(game, PD_MSG_NEUTRAL, PD_STR_WAIT);
    end_turn(game);
}

void pd_game_hero_search(pd_game_t *game) {
    int found = 0;
    const int radius = game->hero.cls == 1 ? 2 : 1;
    if (game->phase != PD_PHASE_PLAY) return;
    game->walk_active = 0;
    game->wand_slot = -1;
    for (int dy = -radius; dy <= radius; ++dy) {
        for (int dx = -radius; dx <= radius; ++dx) {
            const int x = game->hero.x + dx;
            const int y = game->hero.y + dy;
            if ((dx == 0 && dy == 0) || !pd_in_bounds(x, y) ||
                !game->level.visible[y * PD_MAP_W + x]) continue;
            const int distance_x = dx < 0 ? -dx : dx;
            const int distance_y = dy < 0 ? -dy : dy;
            effect_add(game, PD_EFFECT_SEARCH, x, y,
                       (distance_x > distance_y ? distance_x : distance_y) - 1);
        }
    }
    for (int dy = -radius; dy <= radius; ++dy) {
        for (int dx = -radius; dx <= radius; ++dx) {
            const int x = game->hero.x + dx;
            const int y = game->hero.y + dy;
            if (!pd_in_bounds(x, y) ||
                !game->level.visible[y * PD_MAP_W + x]) continue;
            if (!pd_tile_trap(pd_tile_at(&game->level, x, y))) continue;
            if (game->level.known[y * PD_MAP_W + x]) continue;
            game->level.known[y * PD_MAP_W + x] = 1;
            ++found;
        }
    }
    if (found > 0)
        message_simple(game, PD_MSG_GOOD, PD_STR_SEARCH_TRAP);
    else
        message_simple(game, PD_MSG_NEUTRAL, PD_STR_SEARCH);
    end_turn(game);
    if (game->phase == PD_PHASE_PLAY) {
        game->hero.hunger = game->hero.hunger > 4 ?
                            game->hero.hunger - 4 : 0;
        end_turn(game);
    }
}

void pd_game_shop_buy(pd_game_t *game) {
    const int price = 40 + game->depth * 4;
    if (game->phase != PD_PHASE_SHOP) return;
    if (game->hero.gold < price) {
        message_num(game, PD_MSG_WARN, PD_STR_SHOP_SHORT, price);
        return;
    }
    if (game->bag_count >= PD_BAG_MAX) {
        message_simple(game, PD_MSG_WARN, PD_STR_PACK_FULL);
        return;
    }
    game->hero.gold = (uint16_t)(game->hero.gold - price);
    game->bag[game->bag_count++] = (pd_item_t){PD_ITEM_POTION_HEAL, 1, 0, 0};
    message_num(game, PD_MSG_GOOD, PD_STR_SHOP_BUY, price);
    sound_add(game, PD_SOUND_GOLD);
}

void pd_game_hero_stairs(pd_game_t *game) {
    const uint8_t tile = pd_tile_at(&game->level, game->hero.x, game->hero.y);
    if (game->phase != PD_PHASE_PLAY) return;
    game->walk_active = 0;
    game->wand_slot = -1;
    if (pd_tile_stairs_down(tile)) {
        if (game->depth >= 25) {
            message_simple(game, PD_MSG_INFO, PD_STR_STAIRS_SEALED);
            return;
        }
        pd_game_enter_depth(game, (uint8_t)(game->depth + 1));
        return;
    }
    if (pd_tile_stairs_up(tile)) {
        if (game->depth > 1) {
            pd_game_enter_depth(game, (uint8_t)(game->depth - 1));
        } else {
            if (has_amulet(game)) {
                game->phase = PD_PHASE_WON;
                message_simple(game, PD_MSG_GOOD, PD_STR_AMULET_ASCEND);
            } else {
                message_simple(game, PD_MSG_INFO, PD_STR_EXIT_SEALED);
            }
        }
        return;
    }
    message_simple(game, PD_MSG_INFO, PD_STR_NEED_STAIRS);
}

void pd_game_amulet_exit(pd_game_t *game) {
    if (game->phase == PD_PHASE_AMULET && has_amulet(game))
        game->phase = PD_PHASE_WON;
}

void pd_game_amulet_stay(pd_game_t *game) {
    if (game->phase == PD_PHASE_AMULET) game->phase = PD_PHASE_PLAY;
}

void pd_game_hero_potion(pd_game_t *game) {
    int slot = -1;
    if (game->phase != PD_PHASE_PLAY) return;
    game->wand_slot = -1;
    for (int index = 0; index < game->bag_count; ++index) {
        if (game->bag[index].kind == PD_ITEM_POTION_HEAL) {
            slot = index;
            break;
        }
    }
    if (slot < 0) {
        game->potion_hint = 25;
        message_simple(game, PD_MSG_WARN, PD_STR_NO_POTION);
        return;
    }
    game->walk_active = 0;
    pd_game_bag_use(game, slot);
}

void pd_game_wand_zap(pd_game_t *game, int x, int y) {
    const int slot = game->wand_slot;
    int distance_x = x - game->hero.x;
    int distance_y = y - game->hero.y;
    if (game->phase != PD_PHASE_PLAY || slot < 0 ||
        slot >= game->bag_count ||
        game->bag[slot].kind != PD_ITEM_WAND_MAGIC) return;
    if (distance_x < 0) distance_x = -distance_x;
    if (distance_y < 0) distance_y = -distance_y;
    const int mob = pd_in_bounds(x, y) ? mob_at(game, x, y) : -1;
    if (mob < 0 || !game->level.visible[y * PD_MAP_W + x] ||
        distance_x > PD_FOV_RADIUS || distance_y > PD_FOV_RADIUS) {
        message_simple(game, PD_MSG_WARN, PD_STR_WAND_TARGET);
        return;
    }
    if (game->bag[slot].tier == 0) {
        game->wand_slot = -1;
        message_simple(game, PD_MSG_WARN, PD_STR_WAND_EMPTY);
        return;
    }
    --game->bag[slot].tier;
    game->wand_slot = -1;
    game->walk_active = 0;
    effect_add(game, PD_EFFECT_ZAP, x, y,
               (game->hero.x << 8) | game->hero.y);
    sound_add(game, PD_SOUND_MAGIC);
    damage_mob(game, mob, pd_rng_range(&game->roll_rng,
               4 + game->bag[slot].level, 8 + 2 * game->bag[slot].level));
    end_turn(game);
}

/* ---------------------------------------------------------------------- */
/* Bag                                                                     */
/* ---------------------------------------------------------------------- */

static void bag_remove(pd_game_t *game, int slot) {
    if (game->wand_slot == slot) game->wand_slot = -1;
    else if (game->wand_slot > slot) --game->wand_slot;
    if (slot < 0 || slot >= game->bag_count) return;
    for (int index = slot; index + 1 < game->bag_count; ++index)
        game->bag[index] = game->bag[index + 1];
    --game->bag_count;
    if (game->hero.weapon == slot)
        game->hero.weapon = -1;
    else if (game->hero.weapon > slot)
        --game->hero.weapon;
    if (game->hero.armor == slot)
        game->hero.armor = -1;
    else if (game->hero.armor > slot)
        --game->hero.armor;
}

void pd_game_bag_use(pd_game_t *game, int slot) {
    pd_item_t item;
    pd_text_t line;
    if (slot < 0 || slot >= game->bag_count) return;
    game->wand_slot = -1;
    item = game->bag[slot];
    switch (item.kind) {
        case PD_ITEM_POTION_HEAL: {
            const int heal = 18 + item.level * 6;
            game->hero.hp = (int16_t)(game->hero.hp + heal);
            if (game->hero.hp > game->hero.max_hp)
                game->hero.hp = game->hero.max_hp;
            if (game->hero.poison > 0) {
                game->hero.poison = 0;
                message_simple(game, PD_MSG_GOOD, PD_STR_POISON_CURED);
            }
            message_num(game, PD_MSG_GOOD, PD_STR_DRINK, heal);
            sound_add(game, PD_SOUND_DRINK);
            bag_remove(game, slot);
            break;
        }
        case PD_ITEM_POTION_STRENGTH:
            ++game->hero.str;
            message_simple(game, PD_MSG_GOOD, PD_STR_STRENGTH_UP);
            sound_add(game, PD_SOUND_DRINK);
            bag_remove(game, slot);
            break;
        case PD_ITEM_SCROLL_UPGRADE: {
            int target;
            sound_add(game, PD_SOUND_READ);
            target = game->hero.weapon >= 0 ? game->hero.weapon
                                            : game->hero.armor;
            if (target >= 0 && target < game->bag_count) {
                ++game->bag[target].level;
                message_item(game, PD_MSG_GOOD, PD_STR_UPGRADED,
                             &game->bag[target]);
            } else {
                message_simple(game, PD_MSG_WARN, PD_STR_NOTHING_UPGRADE);
            }
            bag_remove(game, slot);
            break;
        }
        case PD_ITEM_SCROLL_MAP:
            sound_add(game, PD_SOUND_READ);
            pd_level_map_all(&game->level);
            message_simple(game, PD_MSG_GOOD, PD_STR_MAPPED);
            bag_remove(game, slot);
            break;
        case PD_ITEM_FOOD:
            game->hero.hunger = PD_HUNGER_MAX;
            game->hero.hp = (int16_t)(game->hero.hp + 6);
            if (game->hero.hp > game->hero.max_hp)
                game->hero.hp = game->hero.max_hp;
            message_simple(game, PD_MSG_GOOD, PD_STR_EAT);
            sound_add(game, PD_SOUND_EAT);
            bag_remove(game, slot);
            break;
        case PD_ITEM_WEAPON:
            game->hero.weapon = (int8_t)slot;
            message_item(game, PD_MSG_INFO, PD_STR_WIELD, &game->bag[slot]);
            break;
        case PD_ITEM_ARMOR:
            game->hero.armor = (int8_t)slot;
            message_item(game, PD_MSG_INFO, PD_STR_WEAR, &game->bag[slot]);
            break;
        case PD_ITEM_AMULET:
            game->phase = PD_PHASE_AMULET;
            break;
        case PD_ITEM_WAND_MAGIC:
            if (game->bag[slot].tier == 0) {
                message_simple(game, PD_MSG_WARN, PD_STR_WAND_EMPTY);
            } else {
                game->wand_slot = (int8_t)slot;
                game->walk_active = 0;
                message_simple(game, PD_MSG_INFO, PD_STR_WAND_AIM);
            }
            break;
        default:
            break;
    }
}

void pd_game_bag_drop(pd_game_t *game, int slot) {
    pd_item_t item;
    int x = game->hero.x;
    int y = game->hero.y;
    if (slot < 0 || slot >= game->bag_count) return;
    if (game->bag[slot].kind == PD_ITEM_AMULET) {
        message_simple(game, PD_MSG_WARN, PD_STR_AMULET_KEEP);
        return;
    }
    if (ground_at(game, x, y) >= 0) {
        if (!pd_level_nearest_open(&game->level, x, y, 2, &x, &y)) {
            message_simple(game, PD_MSG_WARN, PD_STR_NO_ROOM_DROP);
            return;
        }
    }
    item = game->bag[slot];
    bag_remove(game, slot);
    drop_item_on_floor(game, &item, x, y);
    message_item(game, PD_MSG_INFO, PD_STR_DROP, &item);
}

/* ---------------------------------------------------------------------- */
/* Auto-walk and animation                                                 */
/* ---------------------------------------------------------------------- */

static int hero_adjacent_to(const pd_game_t *game, int x, int y) {
    const int dx = game->hero.x - x;
    const int dy = game->hero.y - y;
    return dx >= -1 && dx <= 1 && dy >= -1 && dy <= 1 && (dx || dy);
}

static int hostile_in_sight(const pd_game_t *game, int radius) {
    for (int index = 0; index < PD_MOBS_MAX; ++index) {
        const pd_mob_t *mob = &game->mobs[index];
        int dx;
        int dy;
        if (mob->type == 0xFF) continue;
        if (!game->level.visible[mob->y * PD_MAP_W + mob->x]) continue;
        dx = mob->x - game->hero.x;
        dy = mob->y - game->hero.y;
        if (dx < 0) dx = -dx;
        if (dy < 0) dy = -dy;
        if (dx <= radius && dy <= radius) return 1;
    }
    return 0;
}

static void walk_stop(pd_game_t *game) {
    if (game->walk_active && game->walk_steps > 1)
        message_simple(game, PD_MSG_INFO, PD_STR_YOU_STOP);
    game->walk_active = 0;
}

void pd_game_tap(pd_game_t *game, int x, int y) {
    int dx;
    int dy;
    static uint8_t path_x[2];
    static uint8_t path_y[2];

    if (game->phase != PD_PHASE_PLAY) return;
    if (!pd_in_bounds(x, y)) return;
    if (game->wand_slot >= 0) {
        pd_game_wand_zap(game, x, y);
        return;
    }
    if (x == game->hero.x && y == game->hero.y) {
        pd_game_hero_wait(game);
        return;
    }
    dx = x - game->hero.x;
    dy = y - game->hero.y;
    if (dx >= -1 && dx <= 1 && dy >= -1 && dy <= 1) {
        game->walk_active = 0;
        hero_act_move(game, dx, dy, 0);
        return;
    }
    game->walk_active = 1;
    game->walk_attack = mob_at(game, x, y) >= 0;
    game->walk_x = (uint8_t)x;
    game->walk_y = (uint8_t)y;
    game->walk_steps = 0;
    if (pd_level_path(&game->level, game->hero.x, game->hero.y, x, y, path_x,
                      path_y, 2) <= 0) {
        game->walk_active = 0;
        message_simple(game, PD_MSG_INFO, PD_STR_NO_WAY);
    }
}

void pd_game_tick(pd_game_t *game) {
    for (int index = 0; index < PD_EFFECTS_MAX; ++index) {
        if (game->effects[index].ttl > 0) --game->effects[index].ttl;
    }
    for (int index = 0; index < PD_MOBS_MAX; ++index) {
        if (game->mobs[index].dying > 0 && --game->mobs[index].dying == 0)
            game->mobs[index].type = 0xFF;
        if (game->mobs[index].attacking > 0) --game->mobs[index].attacking;
        if (game->mobs[index].moving > 0) --game->mobs[index].moving;
    }
    if (game->hero_moving > 0) --game->hero_moving;
    if (game->phase != PD_PHASE_PLAY || !game->walk_active ||
        game->hero_moving > 0) return;
    for (int step = 0; step < 1 && game->walk_active; ++step) {
        int dx = 0;
        int dy = 0;
        if (hostile_in_sight(game, 3)) {
            walk_stop(game);
            break;
        }
        if (hero_adjacent_to(game, game->walk_x, game->walk_y)) {
            if (game->walk_attack) {
                const int mob = mob_at(game, game->walk_x, game->walk_y);
                game->walk_active = 0;
                if (mob >= 0) {
                    hero_attack(game, mob);
                    end_turn(game);
                }
            } else {
                game->walk_active = 0;
            }
            break;
        }
        if (++game->walk_steps > 90) {
            walk_stop(game);
            break;
        }
        {
            static uint8_t path_x[2];
            static uint8_t path_y[2];
            if (pd_level_path(&game->level, game->hero.x, game->hero.y,
                              game->walk_x, game->walk_y, path_x, path_y,
                              1) <= 0) {
                walk_stop(game);
                break;
            }
            dx = (int)path_x[0] - game->hero.x;
            dy = (int)path_y[0] - game->hero.y;
        }
        hero_act_move(game, dx, dy, 1);
        if (game->phase != PD_PHASE_PLAY) break;
    }
}

/* ---------------------------------------------------------------------- */
/* Serialization                                                           */
/* ---------------------------------------------------------------------- */

#define PD_SAVE_MAGIC UINT32_C(0x31534450) /* "PDS1" */
#define PD_SAVE_HEADER 40
#define PD_SAVE_EXPLORED (PD_MAP_TILES / 8)
#define PD_SAVE_GROUND_BYTES (PD_GROUND_MAX * 8)
#define PD_SAVE_BAG_BYTES (PD_BAG_MAX * 4)
#define PD_SAVE_MOB_BYTES (PD_MOBS_MAX * 6)
#define PD_SAVE_CHANGE_BYTES (PD_TILE_CHANGES_MAX * 3)
#define PD_SAVE_BASE_BYTES                                               \
    (PD_SAVE_HEADER + PD_SAVE_EXPLORED + PD_SAVE_GROUND_BYTES +          \
     PD_SAVE_BAG_BYTES + PD_SAVE_MOB_BYTES + PD_SAVE_CHANGE_BYTES)
#define PD_SAVE_STATUS_BYTES 1
#define PD_SAVE_BYTES (PD_SAVE_BASE_BYTES + PD_SAVE_STATUS_BYTES + PD_SAVE_EXPLORED)

static void put_u16(uint8_t *out, uint16_t value) {
    out[0] = (uint8_t)value;
    out[1] = (uint8_t)(value >> 8);
}

static void put_u32(uint8_t *out, uint32_t value) {
    put_u16(out, (uint16_t)value);
    put_u16(out + 2, (uint16_t)(value >> 16));
}

static uint16_t get_u16(const uint8_t *in) {
    return (uint16_t)(in[0] | ((uint16_t)in[1] << 8));
}

static uint32_t get_u32(const uint8_t *in) {
    return (uint32_t)get_u16(in) | ((uint32_t)get_u16(in + 2) << 16);
}

int pd_game_save_summary(const uint8_t *bytes, int length,
                         pd_save_slot_t *summary) {
    if (bytes == NULL || summary == NULL || length < PD_SAVE_BASE_BYTES ||
        get_u32(bytes) != PD_SAVE_MAGIC || bytes[12] < 1 || bytes[12] > 25 ||
        bytes[13] >= 3 || get_u16(bytes + 20) == 0)
        return 0;
    summary->occupied = 1;
    summary->cls = bytes[13];
    summary->depth = bytes[12];
    summary->level = bytes[17];
    summary->deepest = bytes[36];
    summary->kills = bytes[35];
    summary->gold = get_u16(bytes + 24);
    summary->turns = get_u16(bytes + 28);
    return 1;
}

int pd_game_visible_save_slots(const pd_game_t *game,
                               uint8_t output[PD_SAVE_SLOTS]) {
    int count = 0;
    int empty = -1;
    for (int slot = 0; slot < PD_SAVE_SLOTS; ++slot) {
        if (game->slots[slot].occupied)
            output[count++] = (uint8_t)slot;
        else if (empty < 0)
            empty = slot;
    }
    if (empty >= 0) output[count++] = (uint8_t)empty;
    return count;
}

int pd_game_serialize(const pd_game_t *game, uint8_t *out, int capacity) {
    int at = PD_SAVE_HEADER;
    if (capacity < PD_SAVE_BYTES) return 0;
    for (int index = 0; index < PD_SAVE_BYTES; ++index) out[index] = 0;
    put_u32(out + 0, PD_SAVE_MAGIC);
    put_u32(out + 4, game->run_seed);
    put_u32(out + 8, game->roll_rng);
    out[12] = game->depth;
    out[13] = game->hero.cls;
    out[14] = game->hero.x;
    out[15] = game->hero.y;
    out[16] = game->hero.facing;
    out[17] = game->hero.level;
    out[18] = game->hero.xp;
    out[19] = game->hero.str;
    put_u16(out + 20, (uint16_t)game->hero.hp);
    put_u16(out + 22, (uint16_t)game->hero.max_hp);
    put_u16(out + 24, game->hero.gold);
    put_u16(out + 26, (uint16_t)game->hero.hunger);
    put_u16(out + 28, game->turn);
    out[30] = (uint8_t)game->hero.weapon;
    out[31] = (uint8_t)game->hero.armor;
    out[32] = game->bag_count;
    out[33] = game->ground_count;
    out[34] = game->change_count;
    out[35] = game->kills;
    out[36] = game->deepest;
    out[37] = (uint8_t)(game->message_total > 255 ? 255 : game->message_total);
    out[38] = game->generation;
    out[39] = game->hero.keys;
    for (int index = 0; index < PD_SAVE_EXPLORED; ++index) {
        uint8_t bits = 0;
        for (int bit = 0; bit < 8; ++bit) {
            const int tile = index * 8 + bit;
            if (tile < PD_MAP_TILES && game->level.explored[tile])
                bits |= (uint8_t)(1u << bit);
        }
        out[at + index] = bits;
    }
    at += PD_SAVE_EXPLORED;
    for (int index = 0; index < PD_GROUND_MAX; ++index) {
        const pd_ground_t *entry = &game->ground[index];
        uint8_t *record = out + at + index * 8;
        if (index >= game->ground_count || !entry->used) continue;
        record[0] = 1;
        record[1] = entry->x;
        record[2] = entry->y;
        record[3] = entry->item.kind;
        record[4] = entry->item.tier;
        record[5] = entry->item.level;
        put_u16(record + 6, entry->item.gold);
    }
    at += PD_SAVE_GROUND_BYTES;
    for (int index = 0; index < PD_BAG_MAX; ++index) {
        if (index >= game->bag_count) continue;
        out[at + index * 4 + 0] = game->bag[index].kind;
        out[at + index * 4 + 1] = game->bag[index].tier;
        out[at + index * 4 + 2] = game->bag[index].level;
    }
    at += PD_SAVE_BAG_BYTES;
    for (int index = 0; index < PD_MOBS_MAX; ++index) {
        uint8_t *record = out + at + index * 6;
        const pd_mob_t *mob = &game->mobs[index];
        if (mob->type == 0xFF || mob->dying > 0) {
            record[0] = 0xFF;
            continue;
        }
        record[0] = mob->type;
        record[1] = mob->x;
        record[2] = mob->y;
        put_u16(record + 3, (uint16_t)mob->hp);
        record[5] = (uint8_t)((mob->awake & 15u) | (mob->wander << 4));
    }
    at += PD_SAVE_MOB_BYTES;
    for (int index = 0; index < PD_TILE_CHANGES_MAX; ++index) {
        if (index >= game->change_count) continue;
        out[at + index * 3 + 0] = game->changes[index].x;
        out[at + index * 3 + 1] = game->changes[index].y;
        out[at + index * 3 + 2] = game->changes[index].tile;
    }
    out[PD_SAVE_BASE_BYTES] = game->hero.poison;
    for (int index = 0; index < PD_SAVE_EXPLORED; ++index) {
        uint8_t bits = 0;
        for (int bit = 0; bit < 8; ++bit) {
            const int tile = index * 8 + bit;
            if (tile < PD_MAP_TILES && game->level.known[tile])
                bits |= (uint8_t)(1u << bit);
        }
        out[PD_SAVE_BASE_BYTES + PD_SAVE_STATUS_BYTES + index] = bits;
    }
    return PD_SAVE_BYTES;
}

int pd_game_restore(pd_game_t *game, const uint8_t *bytes, int length) {
    int at = PD_SAVE_HEADER;
    if (length < PD_SAVE_BASE_BYTES || get_u32(bytes) != PD_SAVE_MAGIC) return 0;
    if (bytes[12] < 1 || bytes[12] > 25) return 0;
    game->run_seed = get_u32(bytes + 4);
    game->roll_rng = get_u32(bytes + 8);
    game->depth = bytes[12];
    game->generation = bytes[38] <= 3 ? bytes[38] : 0;
    game->hero.cls = bytes[13] < 3 ? bytes[13] : 0;
    game->hero.level = bytes[17] < 1 ? 1 : bytes[17];
    game->hero.xp = bytes[18];
    game->hero.str = bytes[19];
    game->hero.hp = (int16_t)get_u16(bytes + 20);
    game->hero.max_hp = (int16_t)get_u16(bytes + 22);
    if (game->hero.max_hp < 1) game->hero.max_hp = 1;
    if (game->hero.hp > game->hero.max_hp) game->hero.hp = game->hero.max_hp;
    game->hero.gold = get_u16(bytes + 24);
    game->hero.keys = bytes[39];
    game->hero.poison = length > PD_SAVE_BASE_BYTES ?
                        bytes[PD_SAVE_BASE_BYTES] : 0;
    game->hero.hunger = (int16_t)get_u16(bytes + 26);
    game->turn = get_u16(bytes + 28);
    game->hero.weapon = (int8_t)bytes[30];
    game->hero.armor = (int8_t)bytes[31];
    if (game->hero.weapon >= PD_BAG_MAX) game->hero.weapon = -1;
    if (game->hero.armor >= PD_BAG_MAX) game->hero.armor = -1;
    game->bag_count = bytes[32] > PD_BAG_MAX ? PD_BAG_MAX : bytes[32];
    game->ground_count = bytes[33] > PD_GROUND_MAX ? PD_GROUND_MAX : bytes[33];
    game->change_count =
        bytes[34] > PD_TILE_CHANGES_MAX ? PD_TILE_CHANGES_MAX : bytes[34];
    game->kills = bytes[35];
    game->deepest = bytes[36] < game->depth ? game->depth : bytes[36];
    game->message_total = 0;
    game->walk_active = 0;
    game->bag_selected = -1;
    game->wand_slot = -1;
    game->potion_hint = 0;
    game->phase = PD_PHASE_PLAY;
    game->hero_from_x = game->hero.x;
    game->hero_from_y = game->hero.y;
    game->hero_moving = 0;
    for (int index = 0; index < PD_EFFECTS_MAX; ++index)
        game->effects[index].ttl = 0;
    game->hero.facing = bytes[16] < 4 ? bytes[16] : 0;

    /* Rebuild the floor from the deterministic generator, then replay the
     * doors, chests and traps the hero already changed. */
    if (game->generation == 0)
        pd_level_generate_legacy(&game->level, game->run_seed, game->depth);
    else
        pd_level_generate(&game->level, game->run_seed, game->depth);
    place_special_tiles(game);
    if (length >= PD_SAVE_BYTES) {
        for (int index = 0; index < PD_SAVE_EXPLORED; ++index) {
            const uint8_t bits =
                bytes[PD_SAVE_BASE_BYTES + PD_SAVE_STATUS_BYTES + index];
            for (int bit = 0; bit < 8; ++bit)
                game->level.known[index * 8 + bit] =
                    (uint8_t)((bits >> bit) & 1u);
        }
    }
    pd_level_map_all(&game->level);
    for (int index = 0; index < PD_MAP_TILES; ++index)
        game->level.explored[index] = 0;
    for (int index = 0; index < PD_SAVE_EXPLORED; ++index) {
        const uint8_t bits = bytes[at + index];
        for (int bit = 0; bit < 8; ++bit) {
            const int tile = index * 8 + bit;
            if (tile < PD_MAP_TILES && (bits & (1u << bit)) != 0)
                game->level.explored[tile] = 1;
        }
    }
    at += PD_SAVE_EXPLORED;
    for (int index = 0; index < PD_GROUND_MAX; ++index) {
        const uint8_t *record = bytes + at + index * 8;
        pd_ground_t *entry = &game->ground[index];
        entry->used = 0;
        if (index >= game->ground_count || record[0] != 1) continue;
        entry->used = 1;
        entry->x = record[1];
        entry->y = record[2];
        entry->item.kind = record[3];
        entry->item.tier = record[4];
        entry->item.level = record[5];
        entry->item.gold = get_u16(record + 6);
    }
    at += PD_SAVE_GROUND_BYTES;
    for (int index = 0; index < PD_BAG_MAX; ++index) {
        if (index >= game->bag_count) {
            game->bag[index].kind = PD_ITEM_GOLD;
            game->bag[index].tier = 0;
            game->bag[index].level = 0;
            game->bag[index].gold = 0;
            continue;
        }
        game->bag[index].kind = bytes[at + index * 4 + 0];
        game->bag[index].tier = bytes[at + index * 4 + 1];
        game->bag[index].level = bytes[at + index * 4 + 2];
        game->bag[index].gold = 0;
    }
    at += PD_SAVE_BAG_BYTES;
    for (int index = 0; index < PD_MOBS_MAX; ++index) {
        const uint8_t *record = bytes + at + index * 6;
        pd_mob_t *mob = &game->mobs[index];
        if (record[0] == 0xFF || record[0] >= PD_MOB_TYPE_COUNT) {
            mob->type = 0xFF;
            continue;
        }
        mob->type = record[0];
        mob->x = record[1];
        mob->y = record[2];
        mob->hp = (int16_t)get_u16(record + 3);
        mob->awake = record[5] & 15;
        mob->wander = (uint8_t)(record[5] >> 4);
        mob->dying = 0;
        mob->attacking = 0;
        mob->from_x = mob->x;
        mob->from_y = mob->y;
        mob->moving = 0;
    }
    at += PD_SAVE_MOB_BYTES;
    for (int index = 0; index < PD_TILE_CHANGES_MAX; ++index) {
        const uint8_t *record = bytes + at + index * 3;
        uint8_t tile = record[2];
        if (index >= game->change_count) {
            game->changes[index].x = 0;
            game->changes[index].y = 0;
            game->changes[index].tile = 0;
            continue;
        }
        if (game->generation == 0 && game->level.theme > 0 &&
            tile / PD_TILES_PER_THEME != game->level.theme &&
            tile / 24 == game->level.theme &&
            tile % 24 <= PD_TILEK_WALL_DOOR)
            tile = PD_TILE(game->level.theme, tile % 24);
        game->changes[index].x = record[0];
        game->changes[index].y = record[1];
        game->changes[index].tile = tile;
        if (pd_in_bounds(record[0], record[1]))
            game->level.tiles[record[1] * PD_MAP_W + record[0]] = tile;
    }
    if (!pd_in_bounds(game->hero.x, game->hero.y) ||
        !pd_tile_walkable(pd_tile_at(&game->level, bytes[14], bytes[15]))) {
        game->hero.x = game->level.entrance_x;
        game->hero.y = game->level.entrance_y;
    } else {
        game->hero.x = bytes[14];
        game->hero.y = bytes[15];
    }
    pd_level_update_fov(&game->level, game->hero.x, game->hero.y);
    return 1;
}
