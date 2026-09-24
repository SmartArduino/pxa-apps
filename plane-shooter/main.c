#include "pxa_canvas.h"
#include "pxa_app_messages.h"
#include "pxa_game_screen.h"
#include "pxa_game_sfx.h"
#include "pxa_i18n.h"
#include "pxa_storage.h"

int32_t pxa_plane_game_start(const uint8_t *config, uint32_t config_length);
int32_t pxa_plane_game_on_event(const uint8_t *event, uint32_t length);
void pxa_plane_game_stop(uint32_t reason);
void pxa_plane_game_configure(uint8_t weapon_level, uint8_t hull_level,
                              uint8_t ship_model, uint8_t mission,
                              uint8_t module);
void pxa_plane_game_set_screen(const pxa_game_screen_t *screen);
int pxa_plane_game_take_exit_request(void);
int pxa_plane_game_take_result(uint32_t *final_score, uint8_t *stars,
                               uint16_t *reward);
extern uint32_t pxa_arcade_ui_generation;
extern pxa_game_sfx_t pxa_plane_sfx;

#define SHELL_NODE 2u
#define SCREEN_W 296
#define SCREEN_H 240
#define NAV_Y 190
#define CONTENT_TOP 36
#define STORAGE_GET_REQUEST UINT32_C(0x535001)
#define STORAGE_SET_REQUEST UINT32_C(0x535002)
#define SHELL_TICK_MS 33u

enum {
    PAGE_HOME,
    PAGE_HANGAR,
    PAGE_SHOP,
    PAGE_BRIEFING,
    PAGE_RESULTS,
    PAGE_BATTLE,
};

enum {
    NOTICE_NONE,
    NOTICE_FUNDS,
    NOTICE_LOCKED,
    NOTICE_MAXED,
};

static uint8_t draw_data[12 * 1024];
static uint8_t ui_commands[512];
static uint8_t packet[512];
static uint8_t storage_payload[96];
static uint8_t shell_initialized;
static uint8_t page;
static uint8_t weapon_level;
static uint8_t hull_level;
static uint8_t ship_model;
static uint8_t story_seen;
static uint8_t notice;
static uint8_t selected_mission;
static uint8_t shop_item;
static uint8_t owned_ships;
static uint8_t owned_modules;
static uint8_t equipped_module;
static uint8_t unlocked_missions;
static uint8_t mission_stars[3];
static uint16_t credits;
static uint32_t last_result_score;
static uint16_t last_result_reward;
static uint8_t last_result_stars;
static uint8_t last_result_mission;
static uint8_t last_result_unlocked;
static int16_t page_scroll[4];
static int16_t pointer_down_x;
static int16_t pointer_down_y;
static int16_t pointer_last_y;
static uint8_t scroll_pointer_active;
static uint8_t scroll_pointer_moved;
static pxa_i18n_t i18n;

/* The shell keeps its 296x240 design coordinates and hit zones; on a larger
 * panel the canvas node is pinned to a centered design-size region after the
 * first present, so input stays in design pixels without a second transform.
 * The battle module reflows its own field to the real screen. */
typedef struct {
    int16_t node_x;
    int16_t node_y;
    int16_t node_w;
    int16_t node_h;
} shell_layout_t;

static shell_layout_t shell_layout;
static pxa_game_screen_t game_screen;
static uint8_t shell_geometry_ready;

static void shell_layout_update(void) {
    const int safe_w = (int)game_screen.width - (int)game_screen.safe_left -
                       (int)game_screen.safe_right;
    const int safe_h = (int)game_screen.height - (int)game_screen.safe_top -
                       (int)game_screen.safe_bottom;
    int width = SCREEN_W;
    int height = SCREEN_H;
    if (width > (int)game_screen.width) width = (int)game_screen.width;
    if (height > (int)game_screen.height) height = (int)game_screen.height;
    shell_layout.node_w = (int16_t)width;
    shell_layout.node_h = (int16_t)height;
    shell_layout.node_x = (int16_t)((int)game_screen.safe_left +
                                    (safe_w - width) / 2);
    shell_layout.node_y = (int16_t)((int)game_screen.safe_top +
                                    (safe_h - height) / 2);
    if (shell_layout.node_x < 0) shell_layout.node_x = 0;
    if (shell_layout.node_y < 0) shell_layout.node_y = 0;
}

/* PATCH the auto-created canvas node to the centered design region. The
 * transaction consumes the next surface generation; later canvas frames
 * continue from there. */
static void apply_shell_geometry(void) {
    pxa_ui_transaction_t transaction = {0};
    uint32_t generation;
    if (shell_geometry_ready || pxa_arcade_ui_generation == 0) return;
    generation = pxa_arcade_ui_generation + 1u;
    if (generation == 0) return;
    if (pxa_ui_transaction_begin(&transaction, generation,
                                 PXA_UI_TRANSACTION_PATCH, packet,
                                 sizeof(packet)) &&
        pxa_ui_set_u8(&transaction, SHELL_NODE, PXA_UI_PROPERTY_POSITION, 1) &&
        pxa_ui_set_length(&transaction, SHELL_NODE, PXA_UI_PROPERTY_X,
                          PXA_UI_LENGTH_LOGICAL_PX, shell_layout.node_x) &&
        pxa_ui_set_length(&transaction, SHELL_NODE, PXA_UI_PROPERTY_Y,
                          PXA_UI_LENGTH_LOGICAL_PX, shell_layout.node_y) &&
        pxa_ui_set_length(&transaction, SHELL_NODE, PXA_UI_PROPERTY_WIDTH,
                          PXA_UI_LENGTH_LOGICAL_PX, shell_layout.node_w) &&
        pxa_ui_set_length(&transaction, SHELL_NODE, PXA_UI_PROPERTY_HEIGHT,
                          PXA_UI_LENGTH_LOGICAL_PX, shell_layout.node_h) &&
        pxa_ui_transaction_commit(&transaction)) {
        pxa_arcade_ui_generation = generation;
        shell_geometry_ready = 1;
        return;
    }
    if (transaction.active) (void)pxa_ui_transaction_cancel(&transaction);
}

static const char hero_asset[] = "assets/ui/home-hero.png";
static const char mission_icon[] = "assets/ui/icon-mission.png";
static const char hangar_icon[] = "assets/ui/icon-hangar.png";
static const char shop_icon[] = "assets/ui/icon-shop.png";
static const char comms_icon[] = "assets/ui/icon-comms.png";
static const char ship_asset[] = "assets/plane-shooter/player-plane-left.png";
static const char ship_mk2_asset[] = "assets/ui/ship-mk2.png";
static const char module_cannon_asset[] = "assets/plane-shooter/pickup-overdrive.png";
static const char module_shield_asset[] = "assets/plane-shooter/pickup-shield.png";

const char *pxa_plane_message(pxa_i18n_message_id_t id) {
    return pxa_i18n_cstr(&i18n, id);
}

size_t pxa_plane_text_size(const char *text) {
    size_t size = 0;
    while (text != NULL && text[size] != '\0') ++size;
    return size;
}

static const char *mission_name(uint8_t index) {
    static const pxa_i18n_message_id_t messages[] = {
        PXA_MSG_MISSION_ONE_NAME, PXA_MSG_MISSION_TWO_NAME,
        PXA_MSG_MISSION_THREE_NAME};
    return pxa_plane_message(messages[index]);
}

static const char *mission_boss(uint8_t index) {
    static const pxa_i18n_message_id_t messages[] = {
        PXA_MSG_MISSION_ONE_BOSS, PXA_MSG_MISSION_TWO_BOSS,
        PXA_MSG_MISSION_THREE_BOSS};
    return pxa_plane_message(messages[index]);
}

static const char *mission_threat(uint8_t index) {
    static const pxa_i18n_message_id_t messages[] = {
        PXA_MSG_MISSION_ONE_THREAT, PXA_MSG_MISSION_TWO_THREAT,
        PXA_MSG_MISSION_THREE_THREAT};
    return pxa_plane_message(messages[index]);
}

static const pxa_game_music_note_t home_music[] = {
    PXA_MUSIC_NOTE(PXA_MUSIC_C5, 2), PXA_MUSIC_NOTE(PXA_MUSIC_E5, 2),
    PXA_MUSIC_NOTE(PXA_MUSIC_G5, 3), PXA_MUSIC_NOTE(PXA_MUSIC_E5, 1),
    PXA_MUSIC_NOTE(PXA_MUSIC_D5, 2), PXA_MUSIC_NOTE(PXA_MUSIC_A4, 2),
    PXA_MUSIC_NOTE(PXA_MUSIC_C5, 3), PXA_MUSIC_NOTE(PXA_MUSIC_REST, 1),
};
static const pxa_game_music_note_t hangar_music[] = {
    PXA_MUSIC_NOTE(PXA_MUSIC_A3, 1), PXA_MUSIC_NOTE(PXA_MUSIC_E4, 1),
    PXA_MUSIC_NOTE(PXA_MUSIC_A4, 2), PXA_MUSIC_NOTE(PXA_MUSIC_C5, 1),
    PXA_MUSIC_NOTE(PXA_MUSIC_E4, 1), PXA_MUSIC_NOTE(PXA_MUSIC_G4, 2),
    PXA_MUSIC_NOTE(PXA_MUSIC_D4, 1), PXA_MUSIC_NOTE(PXA_MUSIC_E4, 3),
};
static const pxa_game_music_note_t shop_music[] = {
    PXA_MUSIC_NOTE(PXA_MUSIC_E5, 1), PXA_MUSIC_NOTE(PXA_MUSIC_G5, 1),
    PXA_MUSIC_NOTE(PXA_MUSIC_A5, 2), PXA_MUSIC_NOTE(PXA_MUSIC_G5, 1),
    PXA_MUSIC_NOTE(PXA_MUSIC_E5, 1), PXA_MUSIC_NOTE(PXA_MUSIC_D5, 2),
    PXA_MUSIC_NOTE(PXA_MUSIC_C5, 1), PXA_MUSIC_NOTE(PXA_MUSIC_E5, 3),
};
static const pxa_game_music_note_t comms_music[] = {
    PXA_MUSIC_NOTE(PXA_MUSIC_D4, 2), PXA_MUSIC_NOTE(PXA_MUSIC_A4, 2),
    PXA_MUSIC_NOTE(PXA_MUSIC_C5, 3), PXA_MUSIC_NOTE(PXA_MUSIC_REST, 1),
    PXA_MUSIC_NOTE(PXA_MUSIC_F4, 2), PXA_MUSIC_NOTE(PXA_MUSIC_C5, 2),
    PXA_MUSIC_NOTE(PXA_MUSIC_D5, 3), PXA_MUSIC_NOTE(PXA_MUSIC_REST, 1),
};
static const pxa_game_music_note_t results_music[] = {
    PXA_MUSIC_NOTE(PXA_MUSIC_C5, 1), PXA_MUSIC_NOTE(PXA_MUSIC_E5, 1),
    PXA_MUSIC_NOTE(PXA_MUSIC_G5, 2), PXA_MUSIC_NOTE(PXA_MUSIC_C6, 3),
    PXA_MUSIC_NOTE(PXA_MUSIC_G5, 1), PXA_MUSIC_NOTE(PXA_MUSIC_A5, 2),
    PXA_MUSIC_NOTE(PXA_MUSIC_G5, 1), PXA_MUSIC_NOTE(PXA_MUSIC_E5, 3),
};

#define SHELL_SONG(notes, tempo, kick, snare, hats, ...) \
    {notes, PXA_MUSIC_LENGTH(notes), tempo, kick, snare, hats, \
     {__VA_ARGS__}, NULL, 0, 0}

static const pxa_game_music_song_t shell_songs[] = {
    SHELL_SONG(home_music, 92, 0x0101, 0x0000, 0x1111,
               PXA_MUSIC_C3, PXA_MUSIC_G3, PXA_MUSIC_A2, PXA_MUSIC_E3,
               PXA_MUSIC_F2, PXA_MUSIC_C3, PXA_MUSIC_G2, PXA_MUSIC_D3),
    SHELL_SONG(hangar_music, 108, 0x1111, 0x4040, 0xAAAA,
               PXA_MUSIC_A2, PXA_MUSIC_E3, PXA_MUSIC_A2, PXA_MUSIC_E3,
               PXA_MUSIC_G2, PXA_MUSIC_D3, PXA_MUSIC_E2, PXA_MUSIC_B2),
    SHELL_SONG(shop_music, 116, 0x0101, 0x1010, 0x5555,
               PXA_MUSIC_C3, PXA_MUSIC_E3, PXA_MUSIC_A2, PXA_MUSIC_E3,
               PXA_MUSIC_F2, PXA_MUSIC_C3, PXA_MUSIC_G2, PXA_MUSIC_D3),
    SHELL_SONG(comms_music, 78, 0x0001, 0x0000, 0x1111,
               PXA_MUSIC_D2, PXA_MUSIC_A2, PXA_MUSIC_D2, PXA_MUSIC_A2,
               PXA_MUSIC_F2, PXA_MUSIC_C3, PXA_MUSIC_G2, PXA_MUSIC_D3),
    SHELL_SONG(results_music, 124, 0x1111, 0x4040, 0x5555,
               PXA_MUSIC_C3, PXA_MUSIC_G3, PXA_MUSIC_A2, PXA_MUSIC_E3,
               PXA_MUSIC_F3, PXA_MUSIC_C4, PXA_MUSIC_G2, PXA_MUSIC_D3),
};

static void set_shell_music(uint8_t shell_page) {
    if (shell_page <= PAGE_RESULTS)
        pxa_game_sfx_set_song(&pxa_plane_sfx, &shell_songs[shell_page]);
}

static size_t text_length(const char *text) {
    size_t length = 0;
    while (text[length] != '\0') ++length;
    return length;
}

static int16_t max_page_scroll(void) {
    static const int16_t limits[] = {0, 88, 72, 112};
    return page <= PAGE_BRIEFING ? limits[page] : 0;
}

static int16_t content_y(int16_t y) {
    return (int16_t)(y - (page <= PAGE_BRIEFING ? page_scroll[page] : 0));
}

static void draw_stars(pxa_canvas_frame_t *frame) {
    static const int16_t stars[][2] = {
        {22, 45}, {51, 121}, {88, 28}, {126, 63}, {171, 35},
        {211, 92}, {264, 52}, {276, 145}, {36, 176}, {151, 184},
    };
    for (uint8_t i = 0; i < sizeof(stars) / sizeof(stars[0]); ++i)
        pxa_canvas_circle(frame, stars[i][0], stars[i][1], 1,
                          (i & 1u) ? 0x6BA5C7 : 0xDDF8FF);
}

static void draw_header(pxa_canvas_frame_t *frame) {
    char value[6];
    const size_t value_size = pxa_canvas_u32_text(value, credits);
    const char *title = pxa_plane_message(PXA_MSG_SCREEN_TITLE);
    const char *credit = pxa_plane_message(PXA_MSG_CURRENCY_CREDITS);
    pxa_canvas_rect(frame, 28, 5, 240, 30, 0x102B43, 8);
    pxa_canvas_line(frame, 42, 34, 254, 34, 0x3DAED1, 1);
    pxa_canvas_text_box_role(frame, 44, 7, 118, 25, pxa_canvas_rgba(0xE7F8FF),
                             PXA_CANVAS_FONT_TITLE, PXA_CANVAS_ALIGN_LEFT,
                             PXA_CANVAS_TEXT_ALIGN_MIDDLE, title,
                             text_length(title));
    pxa_canvas_text_box(frame, 172, 8, 40, 23, 0x78AFCF,
                        PXA_CANVAS_ALIGN_RIGHT, PXA_CANVAS_TEXT_ALIGN_MIDDLE,
                        credit, text_length(credit));
    pxa_canvas_text_box(frame, 212, 8, 48, 23, 0xFFE36A,
                        PXA_CANVAS_ALIGN_LEFT, PXA_CANVAS_TEXT_ALIGN_MIDDLE,
                        value, value_size);
}

static void draw_button(pxa_canvas_frame_t *frame, int16_t x, int16_t y,
                        uint16_t width, const char *label, size_t label_size,
                        const char *icon, size_t icon_size, uint32_t color) {
    pxa_canvas_rect(frame, (int16_t)(x + 2), (int16_t)(y + 3), width, 32,
                    0x07121E, 7);
    pxa_canvas_rect(frame, x, y, width, 32, color, 7);
    pxa_canvas_line(frame, (int16_t)(x + 9), (int16_t)(y + 2),
                    (int16_t)(x + width - 9), (int16_t)(y + 2), 0x8DEBFF, 1);
    if (icon != NULL)
        pxa_canvas_image(frame, x + 10, y + 7, 18, 18, 255,
                         PXA_UI_IMAGE_FIT_CONTAIN, icon, icon_size);
    pxa_canvas_text_box(frame, icon != NULL ? x + 34 : x + 8, y + 2,
                        icon != NULL ? width - 42 : width - 16, 28, 0xFFFFFF,
                        PXA_CANVAS_ALIGN_CENTER, PXA_CANVAS_TEXT_ALIGN_MIDDLE,
                        label, label_size);
}

static void draw_compact_button(pxa_canvas_frame_t *frame, int16_t x, int16_t y,
                                uint16_t width, const char *label,
                                size_t label_size, const char *icon,
                                size_t icon_size, uint32_t color) {
    pxa_canvas_rect(frame, (int16_t)(x + 2), (int16_t)(y + 2), width, 26,
                    0x07121E, 6);
    pxa_canvas_rect(frame, x, y, width, 26, color, 6);
    pxa_canvas_line(frame, (int16_t)(x + 8), (int16_t)(y + 2),
                    (int16_t)(x + width - 8), (int16_t)(y + 2), 0xD0C1FF, 1);
    pxa_canvas_image(frame, x + 10, y + 4, 18, 18, 255,
                     PXA_UI_IMAGE_FIT_CONTAIN, icon, icon_size);
    pxa_canvas_text_box(frame, x + 34, y + 1, width - 42, 24, 0xFFFFFF,
                        PXA_CANVAS_ALIGN_CENTER, PXA_CANVAS_TEXT_ALIGN_MIDDLE,
                        label, label_size);
}

static void draw_nav(pxa_canvas_frame_t *frame) {
    const char *labels[] = {
        pxa_plane_message(PXA_MSG_NAV_HOME),
        pxa_plane_message(PXA_MSG_NAV_HANGAR),
        pxa_plane_message(PXA_MSG_NAV_SHOP),
        pxa_plane_message(PXA_MSG_NAV_MISSIONS)};
    const char *icons[] = {mission_icon, hangar_icon, shop_icon, comms_icon};
    const size_t sizes[] = {sizeof(mission_icon) - 1u, sizeof(hangar_icon) - 1u,
                            sizeof(shop_icon) - 1u, sizeof(comms_icon) - 1u};
    pxa_canvas_rect(frame, 25, NAV_Y, 246, 43, 0x0B2135, 9);
    for (uint8_t i = 0; i < 4; ++i) {
        const int16_t x = (int16_t)(32 + i * 59);
        const uint8_t selected = page == i;
        if (selected) pxa_canvas_rect(frame, x - 3, NAV_Y + 3, 54, 37, 0x164B69, 7);
        pxa_canvas_image(frame, x + 14, NAV_Y + 4, 18, 18, 255,
                         PXA_UI_IMAGE_FIT_CONTAIN, icons[i], sizes[i]);
        pxa_canvas_text_box_role(
            frame, x - 1, NAV_Y + 21, 48, 18,
            pxa_canvas_rgba(selected ? 0xFFFFFF : 0x78AFCF),
            PXA_CANVAS_FONT_CAPTION, PXA_CANVAS_ALIGN_CENTER,
            PXA_CANVAS_TEXT_ALIGN_MIDDLE, labels[i], text_length(labels[i]));
    }
}

static void draw_home(pxa_canvas_frame_t *frame) {
    const char *ready = pxa_plane_message(PXA_MSG_HOME_FALCON_READY);
    const char *ready_mk2 = pxa_plane_message(PXA_MSG_HOME_LARK_READY);
    const char *launch = pxa_plane_message(PXA_MSG_ACTION_LAUNCH_MISSION);
    pxa_canvas_image(frame, 18, content_y(41), 260, 82, 255,
                     PXA_UI_IMAGE_FIT_STRETCH,
                     hero_asset, sizeof(hero_asset) - 1u);
    pxa_canvas_text_box(frame, 35, content_y(124), 226, 22, 0xDDF8FF,
                        PXA_CANVAS_ALIGN_CENTER, PXA_CANVAS_TEXT_ALIGN_MIDDLE,
                        ship_model ? ready_mk2 : ready,
                        text_length(ship_model ? ready_mk2 : ready));
    draw_button(frame, 47, content_y(153), 202, launch, text_length(launch),
                mission_icon, sizeof(mission_icon) - 1u, 0x137EAA);
}

static void draw_compact_level(pxa_canvas_frame_t *frame, int16_t x, int16_t y,
                               const char *label, size_t label_size,
                               uint8_t level) {
    pxa_canvas_text_box(frame, x, y, 42, 18, 0xA7D1E8,
                        PXA_CANVAS_ALIGN_LEFT, PXA_CANVAS_TEXT_ALIGN_MIDDLE,
                        label, label_size);
    for (uint8_t i = 0; i < 3; ++i)
        pxa_canvas_rect(frame, (int16_t)(x + 46 + i * 18), y + 6, 14, 7,
                        i <= level ? 0x55D7C1 : 0x18364B, 3);
}

static void draw_hangar(pxa_canvas_frame_t *frame) {
    const char *name = pxa_plane_message(PXA_MSG_SHIP_FALCON);
    const char *name2 = pxa_plane_message(PXA_MSG_SHIP_LARK);
    const char *weapon = pxa_plane_message(PXA_MSG_STAT_POWER);
    const char *hull = pxa_plane_message(PXA_MSG_STAT_ARMOR);
    const char *upgrade_weapon = pxa_plane_message(PXA_MSG_HANGAR_UPGRADE_WEAPON);
    const char *upgrade_hull = pxa_plane_message(PXA_MSG_HANGAR_UPGRADE_HULL);
    const char *modules[] = {
        pxa_plane_message(PXA_MSG_MODULE_STANDARD),
        pxa_plane_message(PXA_MSG_MODULE_CANNONS),
        pxa_plane_message(PXA_MSG_MODULE_SHIELD)};
    const char *loadout = pxa_plane_message(PXA_MSG_HANGAR_LOADOUT);
    const char *weapon_detail = pxa_plane_message(PXA_MSG_HANGAR_WEAPON_DETAIL);
    const char *hull_detail = pxa_plane_message(PXA_MSG_HANGAR_HULL_DETAIL);
    pxa_canvas_rect(frame, 29, content_y(43), 238, 75, 0x10283E, 8);
    pxa_canvas_image(frame, 43, content_y(52), 68, 45, 255,
                     PXA_UI_IMAGE_FIT_CONTAIN,
                     ship_model ? ship_mk2_asset : ship_asset,
                     ship_model ? sizeof(ship_mk2_asset) - 1u : sizeof(ship_asset) - 1u);
    pxa_canvas_text_box(frame, 120, content_y(47), 126, 18, 0xFFFFFF,
                        PXA_CANVAS_ALIGN_LEFT, PXA_CANVAS_TEXT_ALIGN_MIDDLE,
                        ship_model ? name2 : name,
                        text_length(ship_model ? name2 : name));
    pxa_canvas_text_box_role(
        frame, 120, content_y(69), 126, 16, pxa_canvas_rgba(0x7DE5FF),
        PXA_CANVAS_FONT_CAPTION, PXA_CANVAS_ALIGN_LEFT,
        PXA_CANVAS_TEXT_ALIGN_MIDDLE, modules[equipped_module],
        text_length(modules[equipped_module]));
    draw_compact_level(frame, 35, content_y(96), weapon, text_length(weapon),
                       weapon_level);
    draw_compact_level(frame, 151, content_y(96), hull, text_length(hull),
                       hull_level);
    draw_button(frame, 34, content_y(122), 228, upgrade_weapon,
                text_length(upgrade_weapon),
                mission_icon, sizeof(mission_icon) - 1u, 0x175A7C);
    draw_button(frame, 34, content_y(156), 228, upgrade_hull,
                text_length(upgrade_hull),
                hangar_icon, sizeof(hangar_icon) - 1u, 0x175A7C);
    pxa_canvas_text_box_role(
        frame, 38, content_y(205), 220, 18, pxa_canvas_rgba(0x7DE5FF),
        PXA_CANVAS_FONT_CAPTION, PXA_CANVAS_ALIGN_LEFT,
        PXA_CANVAS_TEXT_ALIGN_MIDDLE, loadout, text_length(loadout));
    pxa_canvas_rect(frame, 34, content_y(226), 228, 52, 0x0F263A, 7);
    pxa_canvas_text_box_role(
        frame, 43, content_y(231), 210, 18, pxa_canvas_rgba(0xDCECF5),
        PXA_CANVAS_FONT_CAPTION, PXA_CANVAS_ALIGN_LEFT,
        PXA_CANVAS_TEXT_ALIGN_MIDDLE, weapon_detail,
        text_length(weapon_detail));
    pxa_canvas_text_box_role(
        frame, 43, content_y(252), 210, 18, pxa_canvas_rgba(0xA7D1E8),
        PXA_CANVAS_FONT_CAPTION, PXA_CANVAS_ALIGN_LEFT,
        PXA_CANVAS_TEXT_ALIGN_MIDDLE, hull_detail, text_length(hull_detail));
}

static void draw_shop(pxa_canvas_frame_t *frame) {
    const char *tabs[] = {pxa_plane_message(PXA_MSG_SHOP_TAB_SHIP),
                          pxa_plane_message(PXA_MSG_SHOP_TAB_WEAPON),
                          pxa_plane_message(PXA_MSG_SHOP_TAB_SHIELD)};
    const char *titles[] = {pxa_plane_message(PXA_MSG_SHIP_LARK),
                            pxa_plane_message(PXA_MSG_MODULE_CANNONS),
                            pxa_plane_message(PXA_MSG_MODULE_SHIELD)};
    const char *details[] = {
        pxa_plane_message(PXA_MSG_SHOP_SHIP_DETAIL),
        pxa_plane_message(PXA_MSG_SHOP_CANNONS_DETAIL),
        pxa_plane_message(PXA_MSG_SHOP_SHIELD_DETAIL)};
    const char *buy_labels[] = {
        pxa_plane_message(PXA_MSG_SHOP_BUY_SHIP),
        pxa_plane_message(PXA_MSG_SHOP_BUY_CANNONS),
        pxa_plane_message(PXA_MSG_SHOP_BUY_SHIELD)};
    const char *equip = pxa_plane_message(PXA_MSG_ACTION_EQUIP);
    const char *equipped = pxa_plane_message(PXA_MSG_STATE_EQUIPPED);
    const char *spec_title = pxa_plane_message(PXA_MSG_SHOP_TRAITS);
    const char *specs[] = {
        pxa_plane_message(PXA_MSG_TRAIT_SHIP),
        pxa_plane_message(PXA_MSG_TRAIT_CANNONS),
        pxa_plane_message(PXA_MSG_TRAIT_SHIELD)};
    const char *assets[] = {ship_mk2_asset, module_cannon_asset, module_shield_asset};
    const size_t asset_sizes[] = {sizeof(ship_mk2_asset) - 1u,
                                  sizeof(module_cannon_asset) - 1u,
                                  sizeof(module_shield_asset) - 1u};
    const uint8_t owned = shop_item == 0
                              ? (owned_ships & 2u) != 0
                              : (owned_modules & (uint8_t)(1u << shop_item)) != 0;
    const uint8_t active = shop_item == 0 ? ship_model == 1u
                                          : equipped_module == shop_item;
    pxa_canvas_rect(frame, 34, content_y(43), 228, 108, 0x172642, 9);
    for (uint8_t i = 0; i < 3; ++i) {
        const int16_t x = (int16_t)(40 + i * 73);
        if (shop_item == i)
            pxa_canvas_rect(frame, x, content_y(47), 66, 23, 0x6750A5, 5);
        pxa_canvas_text_box_role(
            frame, x, content_y(48), 66, 20,
            pxa_canvas_rgba(shop_item == i ? 0xFFFFFF : 0x89AAC2),
            PXA_CANVAS_FONT_CAPTION, PXA_CANVAS_ALIGN_CENTER,
            PXA_CANVAS_TEXT_ALIGN_MIDDLE, tabs[i], text_length(tabs[i]));
    }
    pxa_canvas_image(frame, 52, content_y(76), 62, 45, 255,
                     PXA_UI_IMAGE_FIT_CONTAIN,
                     assets[shop_item], asset_sizes[shop_item]);
    pxa_canvas_text_box(frame, 124, content_y(76), 126, 22, 0xFFFFFF,
                        PXA_CANVAS_ALIGN_CENTER, PXA_CANVAS_TEXT_ALIGN_MIDDLE,
                        titles[shop_item], text_length(titles[shop_item]));
    pxa_canvas_text_box_role(
        frame, 120, content_y(99), 132, 42, pxa_canvas_rgba(0xA7D1E8),
        PXA_CANVAS_FONT_CAPTION, PXA_CANVAS_ALIGN_CENTER,
        PXA_CANVAS_TEXT_ALIGN_MIDDLE, details[shop_item],
        text_length(details[shop_item]));
    draw_button(frame, 47, content_y(155), 202,
                active ? equipped : owned ? equip : buy_labels[shop_item],
                text_length(active ? equipped : owned ? equip
                                                       : buy_labels[shop_item]),
                shop_icon, sizeof(shop_icon) - 1u,
                active ? 0x265260 : 0x6750A5);
    pxa_canvas_rect(frame, 34, content_y(201), 228, 55, 0x10283E, 7);
    pxa_canvas_text_box_role(
        frame, 43, content_y(205), 210, 17, pxa_canvas_rgba(0x7DE5FF),
        PXA_CANVAS_FONT_CAPTION, PXA_CANVAS_ALIGN_LEFT,
        PXA_CANVAS_TEXT_ALIGN_MIDDLE, spec_title, text_length(spec_title));
    pxa_canvas_text_box_role(
        frame, 43, content_y(225), 210, 24, pxa_canvas_rgba(0xDCECF5),
        PXA_CANVAS_FONT_CAPTION, PXA_CANVAS_ALIGN_LEFT,
        PXA_CANVAS_TEXT_ALIGN_MIDDLE, specs[shop_item],
        text_length(specs[shop_item]));
}

static void draw_briefing(pxa_canvas_frame_t *frame) {
    const char *map_title = pxa_plane_message(PXA_MSG_MISSION_MAP_TITLE);
    const char *boss_label = pxa_plane_message(PXA_MSG_MISSION_BOSS_LABEL);
    const char *accept = pxa_plane_message(PXA_MSG_ACTION_LAUNCH_MISSION);
    const char *briefing_title = pxa_plane_message(PXA_MSG_MISSION_BRIEFING_TITLE);
    const char *briefing[] = {
        pxa_plane_message(PXA_MSG_MISSION_ONE_BRIEF),
        pxa_plane_message(PXA_MSG_MISSION_TWO_BRIEF),
        pxa_plane_message(PXA_MSG_MISSION_THREE_BRIEF)};
    const char *objectives[] = {
        pxa_plane_message(PXA_MSG_MISSION_ONE_OBJECTIVE),
        pxa_plane_message(PXA_MSG_MISSION_TWO_OBJECTIVE),
        pxa_plane_message(PXA_MSG_MISSION_THREE_OBJECTIVE)};
    static const uint32_t colors[] = {0x55D7C1, 0xFF6B8B, 0xB580FF};
    pxa_canvas_text_box(frame, 43, content_y(39), 210, 24, 0xDDF8FF,
                        PXA_CANVAS_ALIGN_CENTER, PXA_CANVAS_TEXT_ALIGN_MIDDLE,
                        map_title, text_length(map_title));
    pxa_canvas_line(frame, 66, content_y(82), 148, content_y(66), 0x315A73, 2);
    pxa_canvas_line(frame, 148, content_y(66), 230, content_y(84), 0x315A73, 2);
    for (uint8_t i = 0; i < 3; ++i) {
        const int16_t x = (int16_t)(66 + i * 82);
        const int16_t y = i == 1 ? 66 : (int16_t)(82 + i);
        const uint8_t unlocked = (unlocked_missions & (uint8_t)(1u << i)) != 0;
        pxa_canvas_circle(frame, x, content_y(y), selected_mission == i ? 13 : 9,
                          !unlocked ? 0x172A3A
                                    : selected_mission == i ? colors[i]
                                                            : 0x21445B);
        pxa_canvas_circle(frame, x, content_y(y), unlocked ? 4 : 3,
                          unlocked ? 0xEAF9FF : 0x536B7B);
    }
    pxa_canvas_text_box(frame, 52, content_y(98), 134, 18,
                        colors[selected_mission],
                        PXA_CANVAS_ALIGN_LEFT, PXA_CANVAS_TEXT_ALIGN_MIDDLE,
                        mission_name(selected_mission),
                        text_length(mission_name(selected_mission)));
    for (uint8_t i = 0; i < 3u; ++i) {
        const uint32_t color = i < mission_stars[selected_mission]
                                   ? 0xFFE36A : 0x29445A;
        pxa_canvas_circle(frame, (int16_t)(202 + i * 16), content_y(110), 5,
                          color);
    }
    pxa_canvas_text_box_role(
        frame, 43, content_y(120), 210, 18, pxa_canvas_rgba(0xA7D1E8),
        PXA_CANVAS_FONT_CAPTION, PXA_CANVAS_ALIGN_CENTER,
        PXA_CANVAS_TEXT_ALIGN_MIDDLE, mission_threat(selected_mission),
        text_length(mission_threat(selected_mission)));
    pxa_canvas_text_box_role(
        frame, 52, content_y(140), 58, 18, pxa_canvas_rgba(0x789DB7),
        PXA_CANVAS_FONT_CAPTION, PXA_CANVAS_ALIGN_RIGHT,
        PXA_CANVAS_TEXT_ALIGN_MIDDLE, boss_label, text_length(boss_label));
    pxa_canvas_text_box_role(
        frame, 112, content_y(140), 132, 18, pxa_canvas_rgba(0xFFDCE5),
        PXA_CANVAS_FONT_CAPTION, PXA_CANVAS_ALIGN_LEFT,
        PXA_CANVAS_TEXT_ALIGN_MIDDLE, mission_boss(selected_mission),
        text_length(mission_boss(selected_mission)));
    draw_compact_button(frame, 75, content_y(164), 146, accept,
                        text_length(accept), mission_icon,
                        sizeof(mission_icon) - 1u, 0x6750A5);
    pxa_canvas_rect(frame, 34, content_y(205), 228, 94, 0x10283E, 7);
    pxa_canvas_text_box_role(
        frame, 43, content_y(210), 210, 18, pxa_canvas_rgba(0x7DE5FF),
        PXA_CANVAS_FONT_CAPTION, PXA_CANVAS_ALIGN_LEFT,
        PXA_CANVAS_TEXT_ALIGN_MIDDLE, briefing_title,
        text_length(briefing_title));
    pxa_canvas_text_box_role(
        frame, 43, content_y(244), 210, 20, pxa_canvas_rgba(0xDCECF5),
        PXA_CANVAS_FONT_CAPTION, PXA_CANVAS_ALIGN_LEFT,
        PXA_CANVAS_TEXT_ALIGN_MIDDLE, briefing[selected_mission],
        text_length(briefing[selected_mission]));
    pxa_canvas_text_box_role(
        frame, 43, content_y(276), 210, 20, pxa_canvas_rgba(0x55D7C1),
        PXA_CANVAS_FONT_CAPTION, PXA_CANVAS_ALIGN_LEFT,
        PXA_CANVAS_TEXT_ALIGN_MIDDLE, objectives[selected_mission],
        text_length(objectives[selected_mission]));
}

static void draw_results(pxa_canvas_frame_t *frame) {
    const char *title = pxa_plane_message(PXA_MSG_RESULT_TITLE);
    const char *score_label = pxa_plane_message(PXA_MSG_RESULT_SCORE);
    const char *reward_label = pxa_plane_message(PXA_MSG_RESULT_REWARD);
    const char *unlocked = pxa_plane_message(PXA_MSG_RESULT_UNLOCKED);
    const char *complete = pxa_plane_message(PXA_MSG_RESULT_COMPLETE);
    const char *back = pxa_plane_message(PXA_MSG_ACTION_RETURN_BASE);
    char score_text[10];
    char reward_text[10];
    const size_t score_length = pxa_canvas_u32_text(score_text, last_result_score);
    const size_t reward_length = pxa_canvas_u32_text(reward_text, last_result_reward);
    pxa_canvas_rect(frame, 31, 43, 234, 126, 0x10283E, 8);
    pxa_canvas_line(frame, 50, 73, 246, 73, 0x55D7C1, 1);
    pxa_canvas_text_box_role(
        frame, 68, 46, 160, 26, pxa_canvas_rgba(0xE7F8FF),
        PXA_CANVAS_FONT_TITLE, PXA_CANVAS_ALIGN_CENTER,
        PXA_CANVAS_TEXT_ALIGN_MIDDLE, title, text_length(title));
    pxa_canvas_text_box(frame, 52, 77, 192, 20, 0x7DE5FF,
                        PXA_CANVAS_ALIGN_CENTER, PXA_CANVAS_TEXT_ALIGN_MIDDLE,
                        mission_name(last_result_mission),
                        text_length(mission_name(last_result_mission)));
    for (uint8_t i = 0; i < 3u; ++i) {
        const int16_t x = (int16_t)(124 + i * 24);
        const uint32_t color = i < last_result_stars ? 0xFFE36A : 0x29445A;
        pxa_canvas_circle(frame, x, 106, 8, color);
        if (i < last_result_stars)
            pxa_canvas_circle(frame, (int16_t)(x - 2), 104, 2, 0xFFF8C7);
    }
    pxa_canvas_text_box_role(
        frame, 50, 119, 82, 18, pxa_canvas_rgba(0x8EB6D4),
        PXA_CANVAS_FONT_CAPTION, PXA_CANVAS_ALIGN_RIGHT,
        PXA_CANVAS_TEXT_ALIGN_MIDDLE, score_label, text_length(score_label));
    pxa_canvas_text_box_role(
        frame, 139, 119, 70, 18, pxa_canvas_rgba(0xFFFFFF),
        PXA_CANVAS_FONT_CAPTION, PXA_CANVAS_ALIGN_LEFT,
        PXA_CANVAS_TEXT_ALIGN_MIDDLE, score_text, score_length);
    pxa_canvas_text_box_role(
        frame, 50, 136, 82, 18, pxa_canvas_rgba(0x8EB6D4),
        PXA_CANVAS_FONT_CAPTION, PXA_CANVAS_ALIGN_RIGHT,
        PXA_CANVAS_TEXT_ALIGN_MIDDLE, reward_label, text_length(reward_label));
    pxa_canvas_text_box_role(
        frame, 139, 136, 70, 18, pxa_canvas_rgba(0xFFE36A),
        PXA_CANVAS_FONT_CAPTION, PXA_CANVAS_ALIGN_LEFT,
        PXA_CANVAS_TEXT_ALIGN_MIDDLE, reward_text, reward_length);
    pxa_canvas_text_box_role(
        frame, 52, 151, 192, 16,
        pxa_canvas_rgba(last_result_unlocked ? 0x55D7C1 : 0x89AAC2),
        PXA_CANVAS_FONT_CAPTION, PXA_CANVAS_ALIGN_CENTER,
        PXA_CANVAS_TEXT_ALIGN_MIDDLE,
        last_result_unlocked ? unlocked : complete,
        text_length(last_result_unlocked ? unlocked : complete));
    draw_button(frame, 67, 174, 162, back, text_length(back),
                mission_icon, sizeof(mission_icon) - 1u, 0x137EAA);
}

static int render_shell(void) {
    pxa_canvas_frame_t frame;
    const int16_t scroll_max = max_page_scroll();
    pxa_canvas_begin(&frame, draw_data, sizeof(draw_data));
    pxa_canvas_rect(&frame, 0, 0, SCREEN_W, SCREEN_H, 0x071421, 0);
    draw_stars(&frame);
    draw_header(&frame);
    pxa_canvas_clip_push(&frame, 0, CONTENT_TOP, SCREEN_W,
                         (page == PAGE_RESULTS ? SCREEN_H : NAV_Y) - CONTENT_TOP);
    if (page == PAGE_HOME) draw_home(&frame);
    else if (page == PAGE_HANGAR) draw_hangar(&frame);
    else if (page == PAGE_SHOP) draw_shop(&frame);
    else if (page == PAGE_BRIEFING) draw_briefing(&frame);
    else draw_results(&frame);
    pxa_canvas_clip_pop(&frame);
    if (scroll_max > 0) {
        const int16_t thumb_y = (int16_t)(48 + page_scroll[page] * 94 / scroll_max);
        pxa_canvas_rect(&frame, 281, 48, 3, 124, 0x153247, 2);
        pxa_canvas_rect(&frame, 280, thumb_y, 5, 30, 0x62C9E7, 2);
    }
    if (notice != NOTICE_NONE) {
        const char *messages[] = {
            "", pxa_plane_message(PXA_MSG_NOTICE_FUNDS),
            pxa_plane_message(PXA_MSG_NOTICE_LOCKED),
            pxa_plane_message(PXA_MSG_NOTICE_MAXED)};
        pxa_canvas_rect(&frame, 73, 164, 150, 22, 0x351F34, 6);
        pxa_canvas_text_box_role(
            &frame, 79, 165, 138, 20, pxa_canvas_rgba(0xFFB4C1),
            PXA_CANVAS_FONT_CAPTION, PXA_CANVAS_ALIGN_CENTER,
            PXA_CANVAS_TEXT_ALIGN_MIDDLE, messages[notice],
            text_length(messages[notice]));
    }
    if (page != PAGE_RESULTS) draw_nav(&frame);
    if (!pxa_canvas_present(SHELL_NODE, &frame, &pxa_arcade_ui_generation,
                            &shell_initialized, ui_commands,
                            sizeof(ui_commands), packet, sizeof(packet)))
        return 0;
    apply_shell_geometry();
    return 1;
}

static void save_progress(void) {
    static const char key[] = "plane.progress";
    uint8_t value[] = {3, (uint8_t)credits, (uint8_t)(credits >> 8),
                       weapon_level, hull_level, ship_model, story_seen,
                       selected_mission, owned_ships, owned_modules,
                       equipped_module, unlocked_missions, mission_stars[0],
                       mission_stars[1], mission_stars[2]};
    (void)pxa_storage_set(STORAGE_SET_REQUEST, key, sizeof(key) - 1u, value,
                          sizeof(value), storage_payload, sizeof(storage_payload),
                          packet, sizeof(packet));
}

static int start_battle(void) {
    story_seen = 1;
    notice = 0;
    pxa_plane_game_set_screen(&game_screen);
    pxa_plane_game_configure(weapon_level, hull_level, ship_model,
                             selected_mission, equipped_module);
    if (pxa_plane_game_start(NULL, 0) != PXA_STATUS_OK) return 0;
    page = PAGE_BATTLE;
    save_progress();
    return 1;
}

static int return_from_battle(void) {
    uint32_t final_score = 0;
    uint8_t stars = 0;
    uint16_t reward = 0;
    const uint8_t completed = (uint8_t)pxa_plane_game_take_result(
        &final_score, &stars, &reward);
    pxa_plane_game_stop(0);
    shell_initialized = 0;
    if (completed) {
        const uint16_t previous_credits = credits;
        last_result_score = final_score;
        last_result_reward = reward;
        last_result_stars = stars;
        last_result_mission = selected_mission;
        last_result_unlocked = 0;
        if (mission_stars[selected_mission] < stars)
            mission_stars[selected_mission] = stars;
        credits = (uint16_t)(UINT16_MAX - previous_credits < reward
                                 ? UINT16_MAX
                                 : previous_credits + reward);
        if (selected_mission < 2u) {
            const uint8_t next_mask = (uint8_t)(1u << (selected_mission + 1u));
            if ((unlocked_missions & next_mask) == 0u) {
                unlocked_missions |= next_mask;
                selected_mission++;
                last_result_unlocked = 1;
            }
        }
        page = PAGE_RESULTS;
        save_progress();
    } else {
        page = PAGE_HOME;
    }
    set_shell_music(page);
    return render_shell();
}

static int handle_shell_pointer(int16_t x, int16_t y) {
    notice = NOTICE_NONE;
    if (page == PAGE_RESULTS) {
        if (x >= 67 && x < 229 && y >= 171 && y < 211) {
            page = PAGE_HOME;
            set_shell_music(PAGE_HOME);
            pxa_game_sfx_play(&pxa_plane_sfx, PXA_GAME_SFX_ACTION);
            return render_shell();
        }
        return 1;
    }
    if (y >= NAV_Y) {
        if (x < 91) page = PAGE_HOME;
        else if (x < 150) page = PAGE_HANGAR;
        else if (x < 209) page = PAGE_SHOP;
        else page = PAGE_BRIEFING;
        set_shell_music(page);
        pxa_game_sfx_play(&pxa_plane_sfx, PXA_GAME_SFX_TAP);
        return render_shell();
    }
    if (page == PAGE_HOME && x >= 47 && x < 249 && y >= 150 && y < 190)
        return start_battle();
    if (page == PAGE_BRIEFING) {
        if (y >= 50 && y < 101) {
            const uint8_t candidate = x < 107 ? 0u : x < 190 ? 1u : 2u;
            if ((unlocked_missions & (uint8_t)(1u << candidate)) == 0u) {
                notice = NOTICE_LOCKED;
                return render_shell();
            }
            selected_mission = candidate;
            save_progress();
            return render_shell();
        }
        if (x >= 70 && x < 226 && y >= 164 && y < 190)
            return start_battle();
    }
    if (page == PAGE_HANGAR && x >= 34 && x < 262) {
        if (y >= 43 && y < 120 && (owned_ships & 2u) != 0) {
            ship_model ^= 1u;
            save_progress();
        } else if (y >= 120 && y < 154) {
            if (weapon_level < 2u && credits >= 80u) {
                credits -= 80u; ++weapon_level; save_progress();
            } else notice = weapon_level >= 2u ? NOTICE_MAXED : NOTICE_FUNDS;
        } else if (y >= 154 && y < 190) {
            if (hull_level < 2u && credits >= 100u) {
                credits -= 100u; ++hull_level; save_progress();
            } else notice = hull_level >= 2u ? NOTICE_MAXED : NOTICE_FUNDS;
        }
        return render_shell();
    }
    if (page == PAGE_SHOP && y >= 43 && y < 73) {
        shop_item = x < 111 ? 0 : x < 184 ? 1 : 2;
        return render_shell();
    }
    if (page == PAGE_SHOP && x >= 47 && x < 249 && y >= 153 && y < 190) {
        static const uint16_t prices[] = {320u, 180u, 200u};
        const uint8_t mask = shop_item == 0 ? 2u : (uint8_t)(1u << shop_item);
        uint8_t *owned = shop_item == 0 ? &owned_ships : &owned_modules;
        if ((*owned & mask) == 0) {
            if (credits < prices[shop_item]) notice = NOTICE_FUNDS;
            else {
                credits = (uint16_t)(credits - prices[shop_item]);
                *owned |= mask;
                if (shop_item == 0) ship_model = 1;
                else equipped_module = shop_item;
                save_progress();
            }
        } else if (shop_item == 0) {
            ship_model = 1;
            save_progress();
        } else {
            equipped_module = shop_item;
            save_progress();
        }
        return render_shell();
    }
    return 1;
}


int32_t pxa_app_start(const uint8_t *config, uint32_t config_length) {
    static const char key[] = "plane.progress";
    (void)pxa_i18n_init_from_start_config(
        &i18n, &pxa_app_i18n_bundle, config, config_length);
    pxa_game_screen_from_start(&game_screen, config, config_length);
    shell_layout_update();
    shell_geometry_ready = 0;
    credits = 240u;
    weapon_level = 0u;
    hull_level = 0u;
    ship_model = 0u;
    story_seen = 0u;
    notice = NOTICE_NONE;
    selected_mission = 0u;
    shop_item = 0u;
    owned_ships = 1u;
    owned_modules = 1u;
    equipped_module = 0u;
    unlocked_missions = 1u;
    for (uint8_t i = 0; i < 3u; ++i) mission_stars[i] = 0u;
    for (uint8_t i = 0; i < 4u; ++i) page_scroll[i] = 0;
    scroll_pointer_active = 0u;
    page = PAGE_HOME;
    if (!pxa_window_fullscreen()) return PXA_STATUS_BAD_STATE;
    if (!render_shell()) return PXA_STATUS_INTERNAL;
    if (!pxa_clock_set_period(SHELL_TICK_MS)) return PXA_STATUS_INTERNAL;
    pxa_game_sfx_set_theme(&pxa_plane_sfx, PXA_GAME_SFX_THEME_PLANE);
    set_shell_music(PAGE_HOME);
    pxa_game_sfx_start(&pxa_plane_sfx, packet, sizeof(packet));
    (void)pxa_storage_get(STORAGE_GET_REQUEST, key, sizeof(key) - 1u,
                          storage_payload, sizeof(storage_payload), packet,
                          sizeof(packet));
    return PXA_STATUS_OK;
}

int32_t pxa_app_on_event(const uint8_t *event, uint32_t length) {
    pxa_canvas_event_t parsed;
    pxa_ui_pointer_data_t pointer;
    if (!pxa_canvas_parse_event(event, length, &parsed)) return PXA_EVENT_UNHANDLED;
    {
        int locale_result = pxa_i18n_handle_event(&i18n, &parsed);
        if (locale_result != 0) {
            if (page == PAGE_BATTLE)
                return pxa_plane_game_on_event(event, length);
            return locale_result == 1 && !render_shell()
                       ? PXA_STATUS_INTERNAL
                       : PXA_EVENT_HANDLED;
        }
    }
    if (pxa_game_screen_handle_event(&game_screen, &parsed)) {
        shell_layout_update();
        shell_geometry_ready = 0;
        if (page == PAGE_BATTLE)
            return pxa_plane_game_on_event(event, length);
        return render_shell() ? PXA_EVENT_HANDLED : PXA_STATUS_INTERNAL;
    }
    if (page == PAGE_BATTLE) {
        if (parsed.service == PXA_SERVICE_WINDOW &&
            parsed.opcode == PXA_WINDOW_BACK_REQUESTED) {
            return return_from_battle()
                       ? PXA_EVENT_HANDLED : PXA_STATUS_INTERNAL;
        }
        const int32_t result = pxa_plane_game_on_event(event, length);
        if (pxa_plane_game_take_exit_request())
            return return_from_battle()
                       ? PXA_EVENT_HANDLED : PXA_STATUS_INTERNAL;
        return result;
    }
    if (pxa_game_sfx_handle_event(&pxa_plane_sfx, &parsed, packet,
                                  sizeof(packet)))
        return PXA_EVENT_HANDLED;
    if (parsed.service == PXA_SERVICE_CLOCK &&
        parsed.opcode == PXA_CLOCK_TICK && parsed.payload_length == 8) {
        pxa_game_sfx_tick(&pxa_plane_sfx, &parsed);
        return PXA_EVENT_HANDLED;
    }
    if (parsed.service == PXA_SERVICE_STORAGE &&
        parsed.opcode == PXA_STORAGE_GET && parsed.request_id == STORAGE_GET_REQUEST) {
        pxa_storage_get_result_t result;
        if (!pxa_storage_parse_get(&parsed, &result)) return PXA_STATUS_INTERNAL;
        if (result.status == PXA_STATUS_OK && result.value_length >= 7u &&
            result.value[0] >= 1u && result.value[0] <= 3u) {
            credits = (uint16_t)(result.value[1] | ((uint16_t)result.value[2] << 8));
            weapon_level = result.value[3] > 2u ? 2u : result.value[3];
            hull_level = result.value[4] > 2u ? 2u : result.value[4];
            ship_model = result.value[5] > 1u ? 1u : result.value[5];
            story_seen = result.value[6];
            if (result.value[0] >= 2u && result.value_length >= 11u) {
                selected_mission = result.value[7] < 3u ? result.value[7] : 0u;
                owned_ships = result.value[8] | 1u;
                owned_modules = result.value[9] | 1u;
                equipped_module = result.value[10] < 3u ? result.value[10] : 0u;
                if (result.value[0] == 3u && result.value_length >= 15u) {
                    unlocked_missions = result.value[11] & 7u;
                    if (unlocked_missions == 0u) unlocked_missions = 1u;
                    for (uint8_t i = 0; i < 3u; ++i)
                        mission_stars[i] = result.value[12u + i] > 3u
                                               ? 3u : result.value[12u + i];
                } else {
                    /* Version 2 exposed every mission; preserve existing saves. */
                    unlocked_missions = 7u;
                }
                if ((unlocked_missions &
                     (uint8_t)(1u << selected_mission)) == 0u)
                    selected_mission = 0u;
            } else if (ship_model) {
                owned_ships |= 2u;
            }
        }
        return render_shell() ? PXA_EVENT_HANDLED : PXA_STATUS_INTERNAL;
    }
    if (parsed.service == PXA_SERVICE_STORAGE) return PXA_EVENT_HANDLED;
    if (parsed.service == PXA_SERVICE_WINDOW &&
        parsed.opcode == PXA_WINDOW_BACK_REQUESTED) {
        if (page == PAGE_HOME) return PXA_EVENT_UNHANDLED;
        page = PAGE_HOME;
        set_shell_music(PAGE_HOME);
        return render_shell() ? PXA_EVENT_HANDLED : PXA_STATUS_INTERNAL;
    }
    if (!pxa_canvas_parse_pointer(&parsed, SHELL_NODE, &pointer))
        return PXA_EVENT_UNHANDLED;
    if (pointer.x < 25) return PXA_EVENT_UNHANDLED;
    const int16_t x = (int16_t)pointer.x;
    const int16_t y = (int16_t)pointer.y;
    if (pointer.phase == PXA_POINTER_DOWN) {
        if (y >= NAV_Y) {
            scroll_pointer_active = 0;
            return handle_shell_pointer(x, y)
                       ? PXA_EVENT_HANDLED : PXA_STATUS_INTERNAL;
        }
        if (y < CONTENT_TOP) return PXA_EVENT_HANDLED;
        pointer_down_x = x;
        pointer_down_y = y;
        pointer_last_y = y;
        scroll_pointer_active = 1;
        scroll_pointer_moved = 0;
        return PXA_EVENT_HANDLED;
    }
    if (pointer.phase == PXA_POINTER_MOVE && scroll_pointer_active) {
        const int16_t travel = (int16_t)(y - pointer_down_y);
        const int16_t delta = (int16_t)(pointer_last_y - y);
        const int16_t maximum = max_page_scroll();
        int16_t next;
        pointer_last_y = y;
        if (travel <= -4 || travel >= 4) scroll_pointer_moved = 1;
        if (!scroll_pointer_moved || maximum == 0) return PXA_EVENT_HANDLED;
        next = (int16_t)(page_scroll[page] + delta);
        if (next < 0) next = 0;
        if (next > maximum) next = maximum;
        if (next == page_scroll[page]) return PXA_EVENT_HANDLED;
        page_scroll[page] = next;
        return render_shell() ? PXA_EVENT_HANDLED : PXA_STATUS_INTERNAL;
    }
    if (pointer.phase == PXA_POINTER_UP && scroll_pointer_active) {
        const uint8_t moved = scroll_pointer_moved;
        scroll_pointer_active = 0;
        if (moved) return PXA_EVENT_HANDLED;
        return handle_shell_pointer(
                   pointer_down_x,
                   (int16_t)(pointer_down_y +
                             (page <= PAGE_BRIEFING ? page_scroll[page] : 0)))
                   ? PXA_EVENT_HANDLED : PXA_STATUS_INTERNAL;
    }
    if (pointer.phase == PXA_POINTER_CANCEL) {
        scroll_pointer_active = 0;
        return PXA_EVENT_HANDLED;
    }
    return PXA_EVENT_UNHANDLED;
}

void pxa_app_stop(uint32_t reason) {
    if (page == PAGE_BATTLE) pxa_plane_game_stop(reason);
}
