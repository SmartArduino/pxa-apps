/* Draw-list renderer: builds one GameRender raster list per frame.
 *
 * Tiles come from the Shattered Pixel Dungeon tilesets, one region per five
 * floors. Animation is time based: water and embers cycle through atlas
 * frames, actors pick walk frames while the world moves, and remembered
 * terrain is darkened with a fixed-alpha painter overlay. */
#include "render.h"

#include "assets.h"
#include "font.h"
#include "pxa_log.h"
#include "strings.h"
#include "wall_tiles.h"

#define PD_MAX_TILE_INSTANCES 320
#define PD_MAX_SPRITES 200
#define PD_MAX_TEXT_BATCHES 12
#define PD_MAX_TEXT_GLYPHS 96

typedef struct {
    uint16_t color;
    uint8_t slot;
    int count;
    pxa_raster_sprite_instance_t items[PD_MAX_TEXT_GLYPHS];
} pd_text_batch_t;

static pxa_raster_sprite_instance_t g_tiles[PD_MAX_TILE_INSTANCES];
static int g_tile_count;
static pxa_raster_sprite_instance_t g_walls[PD_MAX_TILE_INSTANCES];
static int g_wall_count;
static pxa_raster_sprite_instance_t g_overlay[PD_MAX_TILE_INSTANCES];
static int g_overlay_count;
/* Remembered tiles need their overlay drawn after the whole tile batch, so the
 * rectangles are collected first and emitted once the tiles are in the list. */
#define PD_MAX_SHADES (PD_MAP_W * PD_MAP_H)
static int16_t g_shade[PD_MAX_SHADES][2];
static int g_shade_count;
static int g_world_cell = PD_CELL;
static pxa_raster_sprite_instance_t g_sprites[PD_MAX_SPRITES];
static int g_sprite_count;
static pd_text_batch_t g_text[PD_MAX_TEXT_BATCHES];
static int g_text_count;
static pxa_raster_draw_list_t g_list;
static uint32_t g_capabilities;
static uint32_t g_now_ms;
static void sprite_flush(void);
static void text_flush(void);
static void text_center(int center_x, int y, const char *text,
                        uint16_t color, int scale);

static uint16_t rgb565(uint8_t red, uint8_t green, uint8_t blue) {
    return (uint16_t)(((uint16_t)(red >> 3) << 11) |
                      ((uint16_t)(green >> 2) << 5) | (blue >> 3));
}

/* ---------------------------------------------------------------------- */
/* Primitives                                                              */
/* ---------------------------------------------------------------------- */

static void draw_rect(int x, int y, int width, int height, uint16_t color) {
    int16_t xy[8];
    if (width <= 0 || height <= 0) return;
    sprite_flush();
    text_flush();
    xy[0] = xy[6] = (int16_t)(x * 16);
    xy[1] = xy[3] = (int16_t)(y * 16);
    xy[2] = xy[4] = (int16_t)((x + width) * 16);
    xy[5] = xy[7] = (int16_t)((y + height) * 16);
    (void)pxa_raster_flat_quad(&g_list, xy, color);
}

static void draw_frame_rect(int x, int y, int width, int height, int border,
                            uint16_t fill, uint16_t edge) {
    draw_rect(x, y, width, height, edge);
    draw_rect(x + border, y + border, width - border * 2, height - border * 2,
              fill);
}

static void draw_shade(int x, int y, int width, int height) {
    pxa_raster_vertex_t vertices[4];
    for (int index = 0; index < 4; ++index) {
        vertices[index].x_q4 = (int16_t)((index == 1 || index == 2 ? x + width : x) * 16);
        vertices[index].y_q4 = (int16_t)((index >= 2 ? y + height : y) * 16);
        vertices[index].u_q4 = 0;
        vertices[index].v_q4 = 0;
        vertices[index].light = 0; /* only one palette row is uploaded */
        vertices[index].depth_q8 = 0;
    }
    (void)pxa_raster_textured_quad_flags(
        &g_list, vertices, PD_TEXTURE_FOG,
        PXA_RASTER_QUAD_PAINTER | PXA_RASTER_QUAD_BLEND_75);
    g_list.required_capabilities |= PXA_RASTER_CAP_FIXED_ALPHA_BLEND;
}

/* Remembered tiles need their overlay drawn after the whole tile batch, so the
 * rectangles are collected first and emitted once the tiles are in the list. */
static void shade_add(int x, int y) {
    if (g_shade_count >= PD_MAX_SHADES) return;
    g_shade[g_shade_count][0] = (int16_t)x;
    g_shade[g_shade_count][1] = (int16_t)y;
    ++g_shade_count;
}

static void shade_flush(void) {
    for (int index = 0; index < g_shade_count; ++index)
        draw_shade(g_shade[index][0], g_shade[index][1], g_world_cell,
                   g_world_cell);
    g_shade_count = 0;
}


static uint8_t g_sprite_slot = PD_TEXTURE_SPRITES;

static void sprite_flush(void) {
    if (g_sprite_count == 0) return;
    (void)pxa_raster_sprite_batch(&g_list, g_sprite_slot,
                                  PXA_RASTER_SPRITE_TRANSPARENT_INDEX0,
                                  g_capabilities, g_sprites,
                                  (uint16_t)g_sprite_count, 0);
    g_sprite_count = 0;
}

/* Interface slices and world sprites live in different textures, so the
 * pending batch is flushed whenever the target texture changes. */
static void sprite_use_slot(uint8_t slot) {
    text_flush();
    if (g_sprite_slot == slot) return;
    sprite_flush();
    g_sprite_slot = slot;
}

static void sprite_instance(int x, int y, int width, int height,
                            uint16_t source_x, uint16_t source_y,
                            uint16_t source_width, uint16_t source_height) {
    if (g_sprite_count >= PD_MAX_SPRITES) sprite_flush();
    g_sprites[g_sprite_count].x = (int16_t)x;
    g_sprites[g_sprite_count].y = (int16_t)y;
    g_sprites[g_sprite_count].width = (uint16_t)width;
    g_sprites[g_sprite_count].height = (uint16_t)height;
    g_sprites[g_sprite_count].source_x = source_x;
    g_sprites[g_sprite_count].source_y = source_y;
    g_sprites[g_sprite_count].source_width = source_width;
    g_sprites[g_sprite_count].source_height = source_height;
    ++g_sprite_count;
}

/* One cell of the sprite atlas. */
static void sprite_add(uint8_t id, int x, int y, int width, int height) {
    sprite_use_slot(PD_TEXTURE_SPRITES);
    sprite_instance(x, y, width, height,
                    (uint16_t)((id % PD_CELLS_PER_ROW) * PD_CELL),
                    (uint16_t)((id / PD_CELLS_PER_ROW) * PD_CELL), PD_CELL,
                    PD_CELL);
}

/* One interface slice from pd_ui_rect, scaled to the requested size. */
static void ui_sprite(uint8_t slice, int x, int y, int width, int height) {
    const uint16_t *rect;
    if (slice >= PD_UI_SLICE_COUNT) return;
    rect = pd_ui_rect[slice];
    sprite_use_slot(PD_TEXTURE_UI);
    sprite_instance(x, y, width, height, rect[0], rect[1], rect[2], rect[3]);
}

static void ui_sprite_without_header(uint8_t slice, int x, int y,
                                     int width, int height) {
    const uint16_t *rect = pd_ui_rect[slice];
    sprite_use_slot(PD_TEXTURE_UI);
    sprite_instance(x, y, width, height, rect[0], rect[1] + 7,
                    rect[2], rect[3] - 7);
}

static void ui_ninepatch(uint8_t slice, int x, int y, int width, int height,
                         int border) {
    const uint16_t *source = pd_ui_rect[slice];
    const int center_w = width - 2 * border;
    const int center_h = height - 2 * border;
    const int source_w = source[2] - 2 * border;
    const int source_h = source[3] - 2 * border;
    const int dest_x[3] = {x, x + border, x + width - border};
    const int dest_y[3] = {y, y + border, y + height - border};
    const int dest_w[3] = {border, center_w, border};
    const int dest_h[3] = {border, center_h, border};
    const int src_x[3] = {source[0], source[0] + border,
                          source[0] + source[2] - border};
    const int src_y[3] = {source[1], source[1] + border,
                          source[1] + source[3] - border};
    const int src_w[3] = {border, source_w, border};
    const int src_h[3] = {border, source_h, border};
    sprite_use_slot(PD_TEXTURE_UI);
    for (int row = 0; row < 3; ++row)
        for (int column = 0; column < 3; ++column)
            sprite_instance(dest_x[column], dest_y[row], dest_w[column],
                            dest_h[row], src_x[column], src_y[row],
                            src_w[column], src_h[row]);
}

static void ui_button(const pd_rect_t *rect, const char *label) {
    ui_ninepatch(PD_UI_BUTTON, rect->x, rect->y, rect->w, rect->h, 4);
    text_center(rect->x + rect->w / 2, rect->y + (rect->h - 12) / 2,
                label, rgb565(238, 234, 220), 1);
}

static void ui_title_button(const pd_rect_t *rect, const char *label,
                            int icon, int icon_width, int icon_height) {
    const int icon_size = rect->h >= 40 ? 20 : 16;
    ui_ninepatch(PD_UI_BUTTON, rect->x, rect->y, rect->w, rect->h, 4);
    ui_sprite(icon, rect->x + 5,
              rect->y + (rect->h - icon_size) / 2,
              icon_width * icon_size / 16,
              icon_height * icon_size / 16);
    text_center(rect->x + (rect->w + icon_size + 5) / 2,
                rect->y + (rect->h - 12) / 2,
                label, rgb565(238, 234, 220), 1);
}

static void ui_scene_back(const pd_rect_t *rect) {
    const int width = rect->w >= 40 ? 23 : 17;
    const int height = rect->w >= 40 ? 17 : 13;
    ui_sprite(PD_UI_EXIT, rect->x + (rect->w - width) / 2,
              rect->y + (rect->h - height) / 2, width, height);
}

static void ui_panel_close(const pd_rect_t *rect) {
    const int size = rect->w >= 40 ? 18 : 15;
    ui_sprite(PD_UI_CLOSE, rect->x + (rect->w - size) / 2,
              rect->y + (rect->h - size) / 2, size, size);
}

static void ui_red_button(const pd_rect_t *rect, const char *label) {
    ui_ninepatch(PD_UI_RED_BUTTON, rect->x, rect->y, rect->w, rect->h, 2);
    text_center(rect->x + rect->w / 2, rect->y + (rect->h - 16) / 2,
                label, rgb565(238, 234, 220), 1);
}


static void text_flush(void) {
    for (int index = 0; index < g_text_count; ++index) {
        pd_text_batch_t *batch = &g_text[index];
        if (batch->count == 0) continue;
        (void)pxa_raster_sprite_batch(
            &g_list, batch->slot,
            PXA_RASTER_SPRITE_TRANSPARENT_INDEX0 |
                PXA_RASTER_SPRITE_SOLID_COLOR |
                PXA_RASTER_SPRITE_TEXEL_ALPHA,
            g_capabilities, batch->items, (uint16_t)batch->count, batch->color);
        batch->count = 0;
    }
}

static pd_text_batch_t *text_batch(uint16_t color, uint8_t slot) {
    for (int index = 0; index < g_text_count; ++index)
        if (g_text[index].color == color && g_text[index].slot == slot)
            return &g_text[index];
    if (g_text_count >= PD_MAX_TEXT_BATCHES) {
        /* Never mix slots in one batch: flush and reuse the first entry. */
        text_flush();
        g_text[0].color = color;
        g_text[0].slot = slot;
        return &g_text[0];
    }
    g_text[g_text_count].color = color;
    g_text[g_text_count].slot = slot;
    g_text[g_text_count].count = 0;
    return &g_text[g_text_count++];
}

/* Anti-aliased text: the atlas texel is coverage and solid_color tints it. */
static int text_draw(int x, int y, const char *text, uint16_t color,
                     int scale) {
    int cursor = x;
    const char *walker = text;
    pd_glyph_t glyph;
    int result;
    sprite_flush();
    while ((result = pd_font_next(&walker, &glyph)) != 0) {
        if (result < 0) continue;
        const uint8_t slot = glyph.cjk == 2 ? PD_TEXTURE_FONT_CJK_EXTRA :
                             glyph.cjk ? PD_TEXTURE_FONT_CJK :
                                          PD_TEXTURE_FONT_ASCII;
        pd_text_batch_t *batch = text_batch(color, slot);
        int advance = pd_font_advance(&glyph, scale);
        if (glyph.atlas == NULL) continue;
        if (batch->count >= PD_MAX_TEXT_GLYPHS) text_flush();
        batch = text_batch(color, slot);
        batch->items[batch->count].x = (int16_t)cursor;
        batch->items[batch->count].y = (int16_t)(y + (glyph.cjk ? 0 : scale));
        batch->items[batch->count].width = (uint16_t)(glyph.cell_width * scale);
        batch->items[batch->count].height = (uint16_t)(glyph.cell_height * scale);
        batch->items[batch->count].source_x =
            (uint16_t)((glyph.slot % (glyph.atlas_width / glyph.cell_width)) *
                       glyph.cell_width);
        batch->items[batch->count].source_y =
            (uint16_t)((glyph.slot / (glyph.atlas_width / glyph.cell_width)) *
                       glyph.cell_height);
        batch->items[batch->count].source_width = glyph.cell_width;
        batch->items[batch->count].source_height = glyph.cell_height;
        ++batch->count;
        cursor += advance;
    }
    return cursor - x;
}

static void text_center(int center_x, int y, const char *text, uint16_t color,
                        int scale) {
    text_draw(center_x - pd_font_width(text, scale) / 2, y, text, color, scale);
}

static void text_right(int right, int y, const char *text, uint16_t color,
                       int scale) {
    text_draw(right - pd_font_width(text, scale), y, text, color, scale);
}

static void draw_hud_value(int x, int y, const char *text, uint16_t color,
                           int scale, int spacing, int condensed) {
    static const uint8_t glyphs[11][5] = {
        {3, 5, 5, 5, 6}, {1, 3, 1, 1, 1}, {6, 1, 2, 4, 7},
        {6, 1, 2, 1, 6}, {5, 5, 7, 1, 1}, {7, 4, 6, 1, 6},
        {3, 4, 7, 5, 7}, {7, 1, 2, 4, 4}, {7, 5, 2, 5, 7},
        {7, 5, 7, 1, 6}, {1, 1, 0, 2, 2},
    };
    for (const char *character = text; *character != '\0'; ++character) {
        const int index = *character == '/' ? 10 : *character - '0';
        if (index < 0 || index > 10) continue;
        const int source_width = index == 1 || index == 10 ? 2 : 3;
        const int width = condensed ? 2 : source_width;
        for (int row = 0; row < 5; ++row)
            for (int column = 0; column < width; ++column)
                if (glyphs[index][row] &
                    (source_width == 3 && condensed ?
                         (column == 0 ? 4u : 3u) :
                         (1u << (width - 1 - column))))
                    draw_rect(x + column * scale, y + row * scale,
                              scale, scale, color);
        x += width * scale + spacing;
    }
}

static void draw_hud_value_fit(int x, int center_y, int width, const char *text,
                               uint16_t color, int scale) {
    const pd_hud_digits_t digits = pd_layout_hud_digits(x, width, text,
                                                        scale);
    draw_hud_value(digits.x, center_y - 5 * digits.scale / 2, text, color,
                   digits.scale, digits.spacing, digits.condensed);
}

static char *append_text(char *out, const char *text) {
    while (*text != '\0') *out++ = *text++;
    return out;
}

static char *append_int(char *out, int value) {
    char digits[12];
    int count = 0;
    if (value < 0) {
        *out++ = '-';
        value = -value;
    }
    if (value == 0) {
        *out++ = '0';
        return out;
    }
    while (value > 0 && count < 11) {
        digits[count++] = (char)('0' + value % 10);
        value /= 10;
    }
    while (count > 0) *out++ = digits[--count];
    return out;
}

static int text_width(const char *text, int scale) {
    return pd_font_width(text, scale);
}

/* ---------------------------------------------------------------------- */
/* Tiles                                                                   */
/* ---------------------------------------------------------------------- */

static void tile_flush(void) {
    if (g_tile_count == 0) return;
    (void)pxa_raster_sprite_batch(&g_list, PD_TEXTURE_TILES,
                                  PXA_RASTER_SPRITE_TRANSPARENT_INDEX0,
                                  g_capabilities, g_tiles,
                                  (uint16_t)g_tile_count, 0);
    g_tile_count = 0;
}

static void tile_add(int x, int y, uint8_t tile) {
    const int cell_x = (tile % PD_CELLS_PER_ROW) * PD_CELL;
    const int cell_y = (tile / PD_CELLS_PER_ROW) * PD_CELL;
    if (g_tile_count >= PD_MAX_TILE_INSTANCES) tile_flush();
    g_tiles[g_tile_count].x = (int16_t)x;
    g_tiles[g_tile_count].y = (int16_t)y;
    g_tiles[g_tile_count].width = (uint16_t)g_world_cell;
    g_tiles[g_tile_count].height = (uint16_t)g_world_cell;
    g_tiles[g_tile_count].source_x = (uint16_t)cell_x;
    g_tiles[g_tile_count].source_y = (uint16_t)cell_y;
    g_tiles[g_tile_count].source_width = PD_CELL;
    g_tiles[g_tile_count].source_height = PD_CELL;
    ++g_tile_count;
}

static void wall_flush(void) {
    if (g_wall_count == 0) return;
    (void)pxa_raster_sprite_batch(&g_list, PD_TEXTURE_WALLS,
                                  PXA_RASTER_SPRITE_TRANSPARENT_INDEX0,
                                  g_capabilities, g_walls,
                                  (uint16_t)g_wall_count, 0);
    g_wall_count = 0;
}

static void wall_add(int x, int y, uint8_t wall) {
    pxa_raster_sprite_instance_t *item;
    if (g_wall_count >= PD_MAX_TILE_INSTANCES) wall_flush();
    item = &g_walls[g_wall_count++];
    item->x = (int16_t)x;
    item->y = (int16_t)y;
    item->width = (uint16_t)g_world_cell;
    item->height = (uint16_t)g_world_cell;
    item->source_x = (uint16_t)((wall % PD_CELLS_PER_ROW) * PD_CELL);
    item->source_y = (uint16_t)((wall / PD_CELLS_PER_ROW) * PD_CELL);
    item->source_width = PD_CELL;
    item->source_height = PD_CELL;
}

static void overlay_flush(void) {
    if (g_overlay_count == 0) return;
    (void)pxa_raster_sprite_batch(&g_list, PD_TEXTURE_TILES,
                                  PXA_RASTER_SPRITE_TRANSPARENT_INDEX0,
                                  g_capabilities, g_overlay,
                                  (uint16_t)g_overlay_count, 0);
    g_overlay_count = 0;
}

static void overlay_add(int x, int y, uint8_t tile) {
    pxa_raster_sprite_instance_t *item;
    if (g_overlay_count >= PD_MAX_TILE_INSTANCES) overlay_flush();
    item = &g_overlay[g_overlay_count++];
    item->x = (int16_t)x;
    item->y = (int16_t)y;
    item->width = (uint16_t)g_world_cell;
    item->height = (uint16_t)g_world_cell;
    item->source_x = (uint16_t)((tile % PD_CELLS_PER_ROW) * PD_CELL);
    item->source_y = (uint16_t)((tile / PD_CELLS_PER_ROW) * PD_CELL);
    item->source_width = PD_CELL;
    item->source_height = PD_CELL;
}

static int terrain_alt(int x, int y) {
    const uint32_t hash = (uint32_t)x * UINT32_C(73856093) ^
                          (uint32_t)y * UINT32_C(19349663);
    return ((hash >> 16) & 1u) != 0;
}

/* Water and embers are the two animated terrain kinds. */
static uint8_t animated_tile(uint8_t tile) {
    const uint8_t kind = PD_TILE_KIND(tile);
    const uint8_t theme = (uint8_t)(tile / PD_TILES_PER_THEME);
    if (kind >= PD_TILEK_WATER_A && kind <= PD_TILEK_WATER_D)
        return PD_TILE(theme,
                       (uint8_t)(PD_TILEK_WATER_A + (g_now_ms / 220u) % 4u));
    if (kind == PD_TILEK_EMBERS_A || kind == PD_TILEK_EMBERS_B)
        return PD_TILE(theme,
                       (uint8_t)(PD_TILEK_EMBERS_A + (g_now_ms / 180u) % 2u));
    return tile;
}

static uint8_t visible_tile(const pd_level_t *level, int x, int y) {
    uint8_t tile = level->tiles[y * PD_MAP_W + x];
    if (pd_tile_trap(tile))
        return level->floor_tile;
    return tile;
}

static int world_cell_on_screen(const pd_layout_t *layout, int x, int y) {
    if (x + g_world_cell <= 0 || y + g_world_cell <= 0 ||
        x >= layout->width || y >= layout->height) return 0;
    if (layout->display_shape != 2) return 1;
    const int center_x = layout->width / 2;
    const int center_y = layout->height / 2;
    const int radius = (layout->width < layout->height ?
                        layout->width : layout->height) / 2;
    const int nearest_x = center_x < x ? x :
                          center_x > x + g_world_cell ? x + g_world_cell :
                          center_x;
    const int nearest_y = center_y < y ? y :
                          center_y > y + g_world_cell ? y + g_world_cell :
                          center_y;
    const int dx = nearest_x - center_x;
    const int dy = nearest_y - center_y;
    return dx * dx + dy * dy <= radius * radius;
}

static void draw_map(const pd_game_t *game, const pd_layout_t *layout,
                     int camera_x, int camera_y) {
    const pd_level_t *level = &game->level;
    const int first_x = camera_x >= 0 ? camera_x / layout->tile_pixels :
                        -1 - (-1 - camera_x) / layout->tile_pixels;
    const int first_y = camera_y >= 0 ? camera_y / layout->tile_pixels :
                        -1 - (-1 - camera_y) / layout->tile_pixels;
    const int origin_x = layout->map_x - camera_x;
    const int origin_y = layout->map_y - camera_y;
    g_world_cell = layout->tile_pixels;
    for (int row = 0; row < layout->rows + 2; ++row) {
        const int tile_y = first_y + row;
        if (tile_y < 0 || tile_y >= PD_MAP_H) continue;
        for (int column = 0; column < layout->cols + 2; ++column) {
            const int tile_x = first_x + column;
            int x;
            int y;
            int face;
            uint8_t tile;
            if (tile_x < 0 || tile_x >= PD_MAP_W) continue;
            if (!level->explored[tile_y * PD_MAP_W + tile_x]) continue;
            tile = visible_tile(level, tile_x, tile_y);
            if (PD_TILE_KIND(tile) == PD_TILEK_VOID) continue;
            x = origin_x + tile_x * g_world_cell;
            y = origin_y + tile_y * g_world_cell;
            if (!world_cell_on_screen(layout, x, y)) continue;
            if (pd_tile_wall(tile)) {
                if (!pd_wall_exposed(level, tile_x, tile_y)) continue;
                face = pd_wall_face(level, tile_x, tile_y);
                if (face >= 0) wall_add(x, y, (uint8_t)face);
            } else if (pd_tile_door(tile) && tile_y > 0 &&
                       pd_tile_wall(pd_tile_at(level, tile_x, tile_y - 1))) {
                wall_add(x, y, PD_WALL(level->theme,
                                      PD_WALLK_DOOR_FLOOR_SIDEWAYS));
            } else {
                const uint8_t kind = PD_TILE_KIND(tile);
                if (kind == PD_TILEK_GRASS && terrain_alt(tile_x, tile_y))
                    tile = PD_TILE(level->theme, PD_TILEK_GRASS_ALT);
                if (kind == PD_TILEK_HIGH_GRASS && terrain_alt(tile_x, tile_y))
                    tile = PD_TILE(level->theme, PD_TILEK_HIGH_GRASS_ALT);
                tile_add(x, y, animated_tile(tile));
                if (pd_tile_water(tile))
                    overlay_add(x, y, PD_TILE(level->theme,
                        PD_TILEK_WATER_SHORE_0 +
                        pd_water_shore(level, tile_x, tile_y)));
            }
            if (pd_tile_trap(pd_tile_at(level, tile_x, tile_y)) &&
                level->known[tile_y * PD_MAP_W + tile_x])
                sprite_add(pd_trap_variant(game->run_seed, game->depth,
                           tile_x, tile_y) == 0 ? PD_SPRITE_TRAP_DART :
                           PD_SPRITE_TRAP_BLAST, x, y,
                           g_world_cell, g_world_cell);
            if (!level->visible[tile_y * PD_MAP_W + tile_x])
                shade_add(x, y);
        }
    }
    tile_flush();
    overlay_flush();
    wall_flush();
}

static void draw_upper_walls(const pd_game_t *game, const pd_layout_t *layout,
                             int camera_x, int camera_y) {
    const pd_level_t *level = &game->level;
    const int first_x = camera_x >= 0 ? camera_x / g_world_cell :
                        -1 - (-1 - camera_x) / g_world_cell;
    const int first_y = camera_y >= 0 ? camera_y / g_world_cell :
                        -1 - (-1 - camera_y) / g_world_cell;
    for (int row = 0; row < layout->rows + 2; ++row) {
        const int tile_y = first_y + row;
        if (tile_y < 0 || tile_y + 1 >= PD_MAP_H) continue;
        for (int column = 0; column < layout->cols + 2; ++column) {
            const int tile_x = first_x + column;
            int upper;
            if (tile_x < 0 || tile_x >= PD_MAP_W) continue;
            if (!level->explored[tile_y * PD_MAP_W + tile_x]) continue;
            if ((pd_tile_wall(pd_tile_at(level, tile_x, tile_y)) &&
                 !pd_wall_exposed(level, tile_x, tile_y)) ||
                (pd_tile_wall(pd_tile_at(level, tile_x, tile_y + 1)) &&
                 !pd_wall_exposed(level, tile_x, tile_y + 1))) continue;
            upper = pd_wall_upper(level, tile_x, tile_y);
            const int screen_x = layout->map_x + tile_x * g_world_cell - camera_x;
            const int screen_y = layout->map_y + tile_y * g_world_cell - camera_y;
            if (!world_cell_on_screen(layout, screen_x, screen_y)) continue;
            if (upper >= 0)
                wall_add(screen_x, screen_y, (uint8_t)upper);
            if (PD_TILE_KIND(pd_tile_at(level, tile_x, tile_y + 1)) ==
                PD_TILEK_HIGH_GRASS)
                overlay_add(screen_x, screen_y,
                            PD_TILE(level->theme,
                                    terrain_alt(tile_x, tile_y + 1) ?
                                    PD_TILEK_HIGH_GRASS_OVERHANG_ALT :
                                    PD_TILEK_HIGH_GRASS_OVERHANG));
        }
    }
    wall_flush();
    overlay_flush();
}

static void draw_ground_items(const pd_game_t *game, const pd_layout_t *layout,
                              int camera_x, int camera_y) {
    if (game->lock_x < PD_MAP_W && game->lock_y < PD_MAP_H &&
        PD_TILE_KIND(pd_tile_at(&game->level, game->lock_x, game->lock_y)) ==
            PD_TILEK_DOOR &&
        game->level.visible[game->lock_y * PD_MAP_W + game->lock_x]) {
        const int x = layout->map_x + game->lock_x * g_world_cell - camera_x;
        const int y = layout->map_y + game->lock_y * g_world_cell - camera_y;
        if (x >= layout->map_x && x < layout->map_x + layout->map_w &&
            y >= layout->map_y && y < layout->map_y + layout->map_h)
            sprite_add(PD_SPRITE_ITEM_IRON_KEY, x + g_world_cell / 4,
                       y + g_world_cell / 4, g_world_cell / 2,
                       g_world_cell / 2);
    }
    for (int index = 0; index < game->ground_count; ++index) {
        const pd_ground_t *entry = &game->ground[index];
        const int x = layout->map_x + entry->x * g_world_cell - camera_x;
        const int y = layout->map_y + entry->y * g_world_cell - camera_y -
                      5 * layout->zoom;
        if (!entry->used) continue;
        if (!game->level.visible[entry->y * PD_MAP_W + entry->x]) continue;
        if (x < layout->map_x || x >= layout->map_x + layout->map_w ||
            y + g_world_cell <= layout->map_y ||
            y >= layout->map_y + layout->map_h) continue;
        sprite_add(pd_item_sprite(&entry->item), x, y, g_world_cell, g_world_cell);
    }
}

/* Ground items glimmer like the original: two pixels pulsing over the heap. */
static void draw_item_sparkles(const pd_game_t *game,
                               const pd_layout_t *layout, int camera_x,
                               int camera_y) {
    const uint32_t phase = (g_now_ms / 90u) % 10u;
    if (phase > 6u) return;
    for (int index = 0; index < game->ground_count; ++index) {
        const pd_ground_t *entry = &game->ground[index];
        const int x = layout->map_x + entry->x * g_world_cell - camera_x;
        const int y = layout->map_y + entry->y * g_world_cell - camera_y -
                      5 * layout->zoom;
        if (!entry->used) continue;
        if (!game->level.visible[entry->y * PD_MAP_W + entry->x]) continue;
        if (x < layout->map_x || x >= layout->map_x + layout->map_w ||
            y + g_world_cell <= layout->map_y ||
            y >= layout->map_y + layout->map_h) continue;
        draw_rect(x + (3 + (int)(phase / 3u)) * layout->zoom,
                  y + 3 * layout->zoom, 2 * layout->zoom, 2 * layout->zoom,
                  rgb565(255, 255, 220));
        draw_rect(x + 10 * layout->zoom,
                  y + (4 + (int)(phase / 2u) % 4) * layout->zoom,
                  2 * layout->zoom, 2 * layout->zoom,
                  rgb565(255, 255, 220));
    }
}

static void draw_mobs(const pd_game_t *game, const pd_layout_t *layout,
                      int camera_x, int camera_y) {
    for (int index = 0; index < PD_MOBS_MAX; ++index) {
        const pd_mob_t *mob = &game->mobs[index];
        uint8_t frame;
        int x;
        int y;
        if (mob->type == 0xFF) continue;
        if (!game->level.visible[mob->y * PD_MAP_W + mob->x]) continue;
        x = layout->map_x + mob->x * g_world_cell - camera_x;
        y = layout->map_y + mob->y * g_world_cell - camera_y;
        if (mob->moving > 0) {
            x += ((int)mob->from_x - mob->x) * g_world_cell * mob->moving / 8;
            y += ((int)mob->from_y - mob->y) * g_world_cell * mob->moving / 8;
        }
        y -= 6 * layout->zoom;
        if (x < layout->map_x || x >= layout->map_x + layout->map_w ||
            y + g_world_cell <= layout->map_y ||
            y >= layout->map_y + layout->map_h) continue;
        if (mob->dying > 0) {
            frame = (uint8_t)(PD_MOB_FRAME_DIE + (mob->dying > 10 ? 0 : 1));
        } else if (mob->attacking > 0) {
            frame = PD_MOB_FRAME_ATTACK;
        } else if (mob->awake & 1u) {
            frame = (uint8_t)(PD_MOB_FRAME_RUN + (g_now_ms / 150u) % 2u);
        } else {
            frame = (uint8_t)(PD_MOB_FRAME_IDLE + (g_now_ms / 420u) % 2u);
        }
        sprite_add(PD_SPRITE_MOB(mob->type, frame), x, y,
                   g_world_cell, g_world_cell);
        if (mob->dying > 0) continue;
        if (mob->hp < pd_mob_max_hp(mob->type)) {
            const int width =
                mob->hp * (g_world_cell - 4) / pd_mob_max_hp(mob->type);
            draw_rect(x + 2, y - 3, g_world_cell - 4, 2, rgb565(40, 20, 20));
            draw_rect(x + 2, y - 3, width < 1 ? 1 : width, 2,
                      rgb565(210, 60, 60));
        }
    }
}

static void draw_hero(const pd_game_t *game, const pd_layout_t *layout,
                      int camera_x, int camera_y) {
    int x = layout->map_x + game->hero.x * g_world_cell - camera_x;
    int y = layout->map_y + game->hero.y * g_world_cell - camera_y;
    uint8_t frame = 0;
    int bob = 0;
    if (game->hero_moving > 0) {
        x += ((int)game->hero_from_x - game->hero.x) * g_world_cell *
             game->hero_moving / 12;
        y += ((int)game->hero_from_y - game->hero.y) * g_world_cell *
             game->hero_moving / 12;
        frame = (uint8_t)(2u + (g_now_ms / 120u) % 3u);
        bob = (int)((g_now_ms / 120u) % 2u) * layout->zoom;
    } else {
        frame = (uint8_t)((g_now_ms / 700u) % 4u == 0u ? 1u : 0u);
    }
    y -= 6 * layout->zoom;
    sprite_add(PD_SPRITE_HERO_FACING(game->hero.cls,
               pd_hero_visual_tier(game),
               game->hero.facing == 2, frame), x, y - bob,
               g_world_cell, g_world_cell);
}

static void draw_search_square(int x, int y, int size, int fade) {
    pxa_raster_vertex_t vertices[4];
    const int source_x = fade * 16;
    for (int index = 0; index < 4; ++index) {
        vertices[index].x_q4 = (int16_t)((index == 1 || index == 2 ?
                                          x + size : x) * 16);
        vertices[index].y_q4 = (int16_t)((index >= 2 ? y + size : y) * 16);
        vertices[index].u_q4 = (int16_t)((source_x +
                              (index == 1 || index == 2 ? 16 : 0)) * 16);
        vertices[index].v_q4 = (int16_t)((index >= 2 ? 16 : 0) * 16);
        vertices[index].light = 0;
        vertices[index].depth_q8 = 0;
    }
    (void)pxa_raster_textured_quad_flags(
        &g_list, vertices, PD_TEXTURE_SEARCH,
        PXA_RASTER_QUAD_PAINTER | PXA_RASTER_QUAD_TRANSPARENT_INDEX0 |
        PXA_RASTER_QUAD_BLEND_75);
    g_list.required_capabilities |= PXA_RASTER_CAP_FIXED_ALPHA_BLEND;
}

static void draw_effects(const pd_game_t *game, const pd_layout_t *layout,
                         int camera_x, int camera_y) {
    for (int index = 0; index < PD_EFFECTS_MAX; ++index) {
        const pd_effect_t *effect = &game->effects[index];
        const int x = layout->map_x + effect->x * g_world_cell - camera_x;
        const int y = layout->map_y + effect->y * g_world_cell - camera_y;
        if (effect->ttl == 0) continue;
        if (x < layout->map_x || x >= layout->map_x + layout->map_w ||
            y < layout->map_y || y >= layout->map_y + layout->map_h) continue;
        if (effect->kind == PD_EFFECT_DAMAGE) {
            char text[8];
            char *out = text;
            out = append_int(out, effect->value);
            *out = '\0';
            text_draw(x + g_world_cell / 2 - text_width(text, 1) / 2,
                      y - (20 - effect->ttl) / 3, text, rgb565(255, 236, 160),
                      1);
            continue;
        }
        if (effect->kind == PD_EFFECT_SEARCH) {
            const int age = 20 - effect->ttl - effect->value * 3;
            int size;
            int inset;
            if (age < 0 || age >= 16) continue;
            size = g_world_cell * (16 - age) / 20;
            if (size < layout->zoom) size = layout->zoom;
            inset = (g_world_cell - size) / 2;
            draw_search_square(x + inset, y + inset, size, age / 4);
            continue;
        }
        if (effect->kind == PD_EFFECT_ZAP) {
            const int age = 20 - effect->ttl;
            const int start_x = layout->map_x +
                ((effect->value >> 8) & 0xff) * g_world_cell - camera_x +
                8 * layout->zoom;
            const int start_y = layout->map_y +
                (effect->value & 0xff) * g_world_cell - camera_y +
                8 * layout->zoom;
            const int target_x = x + 8 * layout->zoom;
            const int target_y = y + 8 * layout->zoom;
            if (age < 12) {
                for (int trail = 4; trail >= 0; --trail) {
                    const int progress = age - trail * 2;
                    if (progress < 0) continue;
                    const int center_x = start_x +
                        (target_x - start_x) * progress / 12;
                    const int center_y = start_y +
                        (target_y - start_y) * progress / 12;
                    const int size = (trail == 0 ? 3 : trail < 3 ? 2 : 1) *
                                     layout->zoom;
                    draw_rect(center_x - size / 2, center_y - size / 2,
                              size, size, trail == 0 ? rgb565(255, 255, 255) :
                              rgb565(174, 205, 255));
                }
            } else {
                static const int8_t rays[8][2] = {
                    {1, 0}, {-1, 0}, {0, 1}, {0, -1},
                    {1, 1}, {-1, 1}, {1, -1}, {-1, -1}};
                const int distance = (age - 11) * layout->zoom;
                for (int ray = 0; ray < 8; ++ray)
                    draw_rect(target_x + rays[ray][0] * distance,
                              target_y + rays[ray][1] * distance,
                              layout->zoom, layout->zoom,
                              ray & 1 ? rgb565(158, 195, 255) :
                                        rgb565(255, 255, 255));
            }
            continue;
        }
        if (effect->kind == PD_EFFECT_LIGHTNING) {
            const int age = 20 - effect->ttl;
            const int start_x = layout->map_x +
                ((effect->value >> 8) & 0xff) * g_world_cell - camera_x +
                8 * layout->zoom;
            const int start_y = layout->map_y +
                (effect->value & 0xff) * g_world_cell - camera_y +
                8 * layout->zoom;
            const int target_x = x + 8 * layout->zoom;
            const int target_y = y + 8 * layout->zoom;
            if (age < 12) {
                int previous_x = start_x;
                int previous_y = start_y;
                for (int segment = 1; segment <= 6; ++segment) {
                    const int jitter = segment == 6 ? 0 :
                        ((segment * 7 + age * 3) % 5 - 2) * layout->zoom;
                    const int next_x = start_x +
                        (target_x - start_x) * segment / 6 + jitter;
                    const int next_y = start_y +
                        (target_y - start_y) * segment / 6 - jitter;
                    const int left = previous_x < next_x ? previous_x : next_x;
                    const int top = previous_y < next_y ? previous_y : next_y;
                    const uint16_t color = segment & 1 ?
                        rgb565(245, 252, 183) : rgb565(183, 222, 255);
                    draw_rect(left, previous_y,
                              (previous_x > next_x ? previous_x - next_x :
                               next_x - previous_x) + layout->zoom,
                              layout->zoom, color);
                    draw_rect(next_x, top, layout->zoom,
                              (previous_y > next_y ? previous_y - next_y :
                               next_y - previous_y) + layout->zoom, color);
                    previous_x = next_x;
                    previous_y = next_y;
                }
            }
            continue;
        }
        if (effect->kind == PD_EFFECT_TRAP_BLAST) {
            const int age = 20 - effect->ttl;
            if (age < 15) {
                const int radius = (age + 1) * layout->zoom;
                const int center_x = x + g_world_cell / 2;
                const int center_y = y + g_world_cell / 2;
                const uint16_t color = age < 6 ? rgb565(255, 245, 170) :
                                       rgb565(236, 112, 45);
                draw_rect(center_x - radius, center_y - radius,
                          radius * 2, layout->zoom, color);
                draw_rect(center_x - radius, center_y + radius,
                          radius * 2, layout->zoom, color);
                draw_rect(center_x - radius, center_y - radius,
                          layout->zoom, radius * 2, color);
                draw_rect(center_x + radius, center_y - radius,
                          layout->zoom, radius * 2, color);
            }
            continue;
        }
        if (effect->kind == PD_EFFECT_TRAP_DART ||
            effect->kind == PD_EFFECT_POISON ||
            effect->kind == PD_EFFECT_SWARM) {
            const int age = 20 - effect->ttl;
            if (age < 16) {
                const uint16_t color = effect->kind == PD_EFFECT_TRAP_DART ?
                    rgb565(140, 225, 95) : effect->kind == PD_EFFECT_POISON ?
                    rgb565(94, 185, 68) : rgb565(173, 198, 124);
                for (int particle = 0; particle < 6; ++particle) {
                    const int drift = ((particle * 7 + effect->x * 3 +
                                        effect->y) % 9) - 4;
                    const int horizontal = effect->kind == PD_EFFECT_TRAP_DART ?
                        (particle & 1 ? 1 : -1) * age / 2 :
                        drift * age / 5;
                    const int vertical = effect->kind == PD_EFFECT_TRAP_DART ?
                        (particle / 2 - 1) * age / 3 : -age / 2;
                    draw_rect(x + (8 + horizontal) * layout->zoom,
                              y + (8 + vertical) * layout->zoom,
                              layout->zoom, layout->zoom, color);
                }
            }
            continue;
        }
        if (effect->kind == PD_EFFECT_LEAF) {
            const int age = 40 - effect->ttl;
            const uint32_t seed = (uint32_t)(effect->x * 73 + effect->y * 149);
            if (age >= 34) continue;
            for (int leaf = 0; leaf < 4; ++leaf) {
                const int drift = (leaf & 1 ? 1 : -1) * (2 + age / 5);
                const int sway = (int)((seed + (uint32_t)leaf * 7u +
                                        (uint32_t)age / 4u) % 3u) - 1;
                const int leaf_x = (8 + drift + sway) * layout->zoom;
                const int leaf_y = (8 - age / 2 + age * age / 72 +
                                    (leaf / 2) * 2) * layout->zoom;
                const int size = (age < 16 ? 2 : 1) * layout->zoom;
                const uint16_t color = game->level.theme <= 1 ?
                    (leaf & 1 ? rgb565(93, 155, 63) : rgb565(134, 177, 74)) :
                    (leaf & 1 ? rgb565(157, 129, 74) : rgb565(195, 162, 94));
                draw_rect(x + leaf_x, y + leaf_y, size, size, color);
            }
            continue;
        }
        if (effect->kind == PD_EFFECT_BLOOD) {
            const uint32_t seed = (uint32_t)(effect->x * 73 + effect->y * 149);
            const int age = 20 - effect->ttl;
            for (int dot = 0; dot < 8; ++dot) {
                const int drift_x = (int)((seed + (uint32_t)dot * 47u) % 9u) - 4;
                const int drift_y = (int)((seed + (uint32_t)dot * 23u) % 7u) - 5;
                const int dx = (8 + drift_x * age / 6) * layout->zoom;
                const int dy = (8 + drift_y * age / 6 + age * age / 36) * layout->zoom;
                const uint16_t color =
                    rgb565(190, 40 + (uint8_t)(dot * 4), 40);
                draw_rect(x + dx, y + dy, layout->zoom, layout->zoom, color);
            }
            continue;
        }
        {
            const uint32_t phase = (uint32_t)effect->ttl;
            draw_rect(x + (7 - (int)(phase % 3u)) * layout->zoom,
                      y + 7 * layout->zoom, 3 * layout->zoom, 3 * layout->zoom,
                      rgb565(255, 255, 210));
            draw_rect(x + 6 * layout->zoom,
                      y + (6 - (int)(phase % 4u)) * layout->zoom,
                      2 * layout->zoom, 2 * layout->zoom,
                      rgb565(255, 240, 160));
        }
    }
}

/* ---------------------------------------------------------------------- */
/* Chrome                                                                  */
/* ---------------------------------------------------------------------- */

static void draw_hud(const pd_game_t *game, const pd_layout_t *layout) {
    const pd_hero_t *hero = &game->hero;
    char text[32];
    char *out;
    const int needed = 6 + hero->level * 5;
    const int pane_x = layout->hero_info.x;
    const int pane_y = layout->hero_info.y;
    const int pane_w = layout->hero_info.w;
    const int pane_h = layout->hero_info.h;
    const int value_scale = pane_w > 82 ? 2 : 1;
    int fill;

    /* A status panel shaped like the original's: portrait, health bar and an
     * experience bar along the bottom edge. */
    ui_sprite(PD_UI_STATUS_PANE, pane_x, pane_y, pane_w, pane_h);
    sprite_add(PD_SPRITE_HERO(hero->cls, pd_hero_visual_tier(game), 0),
               pane_x + 7 * pane_w / 82, pane_y + 8 * pane_h / 38,
               16 * pane_w / 82, 16 * pane_h / 38);
    if (hero->hunger <= PD_HUNGER_WARN) {
        const int buff_size = pane_w > 82 ? 11 : 7;
        ui_sprite(hero->hunger == 0 ? PD_UI_BUFF_STARVING : PD_UI_BUFF_HUNGRY,
                  pane_x + 33 * pane_w / 82,
                  pane_y + 12 * pane_h / 38,
                  buff_size, buff_size);
    }
    if (hero->poison > 0) {
        const int buff_size = pane_w > 82 ? 11 : 7;
        ui_sprite(PD_UI_BUFF_POISON,
                  pane_x + 43 * pane_w / 82,
                  pane_y + 12 * pane_h / 38,
                  buff_size, buff_size);
    }

    /* Health bar in the panel's bar recess. */
    fill = hero->max_hp == 0 ? 0 : 50 * hero->hp / hero->max_hp;
    if (fill > 50) fill = 50;
    if (fill > 0)
        ui_sprite(PD_UI_HP_BAR, pane_x + 30 * pane_w / 82,
                  pane_y + 2 * pane_h / 38,
                  fill * pane_w / 82, 8 * pane_h / 38);

    out = text;
    out = append_int(out, hero->hp);
    *out++ = '/';
    out = append_int(out, hero->max_hp);
    *out = '\0';
    draw_hud_value_fit(pane_x + 30 * pane_w / 82,
                       pane_y + 2 * pane_h / 38 + 4 * pane_h / 38,
                       50 * pane_w / 82, text,
                       rgb565(255, 240, 236), value_scale);

    /* Experience bar along the panel's lower edge. */
    if (needed > 0) {
        int width = 17 * hero->xp / needed;
        if (width > 17) width = 17;
        if (width > 0)
            ui_sprite(PD_UI_EXP_BAR, pane_x + 2 * pane_w / 82,
                      pane_y + 30 * pane_h / 38,
                      width * pane_w / 82, 4 * pane_h / 38);
    }

    out = text;
    out = append_int(out, hero->level);
    *out = '\0';
    const int badge_width = 14 * pane_w / 82;
    draw_hud_value_fit(pane_x + 25 * pane_w / 82 - badge_width / 2,
                       pane_y + 31 * pane_h / 38,
                       badge_width, text,
                       rgb565(250, 236, 180), value_scale);
}

static void draw_messages(const pd_game_t *game, const pd_layout_t *layout) {
    const pd_message_t *message;
    const char *walker;
    const char *start;
    const int line_h = pd_strings_chinese() ? 17 : 14;
    const int max_lines = layout->map_h >= 80 ? 2 : 1;
    const int max_width = layout->width - layout->safe_left -
                          layout->safe_right - 8;
    char lines[2][PD_MESSAGE_TEXT];
    int count = 0;
    int line_width = 0;
    int box_y;
    uint16_t color = rgb565(255, 228, 92);
    if (game->message_total == 0 || max_width < 8) return;
    message = &game->messages[(game->message_total - 1u) % PD_MESSAGE_COUNT];
    walker = start = message->text;
    while (*walker && count < max_lines) {
        const char *next = walker;
        pd_glyph_t glyph;
        const int result = pd_font_next(&next, &glyph);
        const int advance = result > 0 ? pd_font_advance(&glyph, 1) : 0;
        if (line_width + advance > max_width) {
            if (walker != start) {
                int length = (int)(walker - start);
                if (length >= PD_MESSAGE_TEXT) length = PD_MESSAGE_TEXT - 1;
                for (int offset = 0; offset < length; ++offset)
                    lines[count][offset] = start[offset];
                lines[count++][length] = '\0';
                start = walker;
                line_width = 0;
                continue;
            }
            break;
        }
        line_width += advance;
        walker = next;
    }
    if (count < max_lines && walker != start) {
        int length = (int)(walker - start);
        if (length >= PD_MESSAGE_TEXT) length = PD_MESSAGE_TEXT - 1;
        for (int offset = 0; offset < length; ++offset)
            lines[count][offset] = start[offset];
        lines[count++][length] = '\0';
    }
    if (count == 0) return;
    box_y = layout->height - layout->safe_bottom - layout->bar_h -
            count * line_h - 2;
    if (message->color == PD_MSG_GOOD) color = rgb565(140, 226, 140);
    if (message->color == PD_MSG_BAD) color = rgb565(255, 120, 110);
    if (message->color == PD_MSG_WARN) color = rgb565(250, 214, 96);
    if (message->color == PD_MSG_INFO) color = rgb565(250, 217, 85);
    for (int index = 0; index < count; ++index)
        text_draw(layout->safe_left + 2, box_y + index * line_h,
                  lines[index], color, 1);
}

/* The original's action toolbar: five slots with the wait, search, potion,
 * pack and stairs actions. */
static void draw_buttons(const pd_game_t *game, const pd_layout_t *layout) {
    char text[16];
    char *out;
    int potions = 0;
    for (int index = 0; index < game->bag_count; ++index)
        if (game->bag[index].kind == PD_ITEM_POTION_HEAL) ++potions;
    for (int index = 0; index < PD_BUTTON_COUNT; ++index) {
        const pd_rect_t *rect = &layout->button[index];
        const int icon = rect->h - 4;
        const int icon_x = rect->x + (rect->w - icon) / 2;
        const int icon_y = rect->y + 2;
        ui_sprite(PD_UI_TOOLBAR_SLOT, rect->x, rect->y, rect->w, rect->h);
        switch (index) {
            case PD_BUTTON_WAIT:
                ui_sprite(PD_UI_TOOLBAR_WAIT, icon_x, icon_y, icon, icon);
                break;
            case PD_BUTTON_SEARCH:
                ui_sprite(PD_UI_TOOLBAR_SEARCH, icon_x, icon_y, icon, icon);
                break;
            case PD_BUTTON_POTION:
            {
                const int potion_size = rect->h >= 40 ? 32 : PD_CELL;
                sprite_add(PD_SPRITE_ITEM_POTION_HEAL,
                           rect->x + (rect->w - potion_size) / 2,
                           rect->y + (rect->h - potion_size) / 2 - 2,
                           potion_size, potion_size);
                out = text;
                out = append_int(out, potions);
                *out = '\0';
                text_right(rect->x + rect->w - 2, rect->y + rect->h - 13, text,
                           potions > 0 ? rgb565(240, 240, 244)
                                       : rgb565(145, 140, 150), 1);
                break;
            }
            case PD_BUTTON_BAG:
                ui_sprite(PD_UI_TOOLBAR_BAG, icon_x, icon_y, icon, icon);
                break;
            default:
                ui_sprite(PD_UI_STAIRS_ICON, icon_x, icon_y, icon, icon);
                break;
        }
    }
}

/* ---------------------------------------------------------------------- */
/* Screens                                                                 */
/* ---------------------------------------------------------------------- */

static void fill_screen(const pd_layout_t *layout, uint16_t color) {
    draw_rect(0, 0, layout->width, layout->height, color);
}

static void draw_victory_stat(pd_string_id_t label, int value,
                              int x, int y, uint16_t color) {
    char text[48];
    pd_str_format(text, sizeof(text), label, value);
    text_center(x, y, text, color, 1);
}

static void draw_victory_screen(const pd_game_t *game,
                                const pd_layout_t *layout) {
    const int available_h = layout->height - layout->safe_top -
                            layout->safe_bottom;
    const int compact = available_h < 210;
    const int center = layout->width / 2;
    const int title_y = layout->safe_top + (compact ?
                        layout->display_shape == 2 ? -8 : 3 :
                        layout->height >= 300 ? 20 : 12);
    const int title_scale = compact ? 2 : 3;
    const int amulet_size = compact ? 18 : layout->height < 300 ? 28 : 32;
    const int amulet_y = title_y + 16 * title_scale + 2;
    const int line_h = compact ? 16 : layout->height < 300 ? 20 : 22;
    const int space = layout->victory_back.y - amulet_y - amulet_size -
                      3 * line_h;
    const int stats_top = amulet_y + amulet_size +
                          (space > 8 ? space / 2 : 4);
    const int column = layout->width < 200 ? 40 : layout->width / 4;
    const uint16_t ink = rgb565(228, 228, 234);
    fill_screen(layout, rgb565(19, 22, 43));
    text_center(center, title_y, pd_str(PD_STR_WIN_TITLE),
                rgb565(255, 224, 144), title_scale);
    sprite_add(PD_SPRITE_ITEM_AMULET, center - amulet_size / 2,
               amulet_y, amulet_size, amulet_size);
    draw_victory_stat(PD_STR_STAT_DEPTH, game->depth,
                      center - column, stats_top, ink);
    draw_victory_stat(PD_STR_STAT_LEVEL, game->hero.level,
                      center + column, stats_top, ink);
    draw_victory_stat(PD_STR_STAT_KILLS, game->kills,
                      center - column, stats_top + line_h, ink);
    draw_victory_stat(PD_STR_STAT_GOLD, game->hero.gold,
                      center + column, stats_top + line_h, ink);
    draw_victory_stat(PD_STR_STAT_TURNS, game->turn,
                      center - column, stats_top + 2 * line_h, ink);
    draw_victory_stat(PD_STR_STAT_DEEPEST, game->deepest,
                      center + column, stats_top + 2 * line_h,
                      rgb565(164, 204, 244));
    ui_button(&layout->victory_back, pd_str(PD_STR_RANKINGS));
}

static void draw_amulet_scene(const pd_layout_t *layout) {
    const int compact = layout->height < 210;
    const int center = layout->width / 2;
    const int sprite_size = compact ? 24 : 36;
    const int sprite_y = layout->amulet_exit.y -
                         (compact ? 72 : 84);
    fill_screen(layout, rgb565(19, 22, 43));
    sprite_add(PD_SPRITE_ITEM_AMULET, center - sprite_size / 2,
               sprite_y, sprite_size, sprite_size);
    text_center(center, sprite_y + sprite_size + (compact ? 5 : 8),
                pd_str(PD_STR_AMULET_LINE_1), rgb565(255, 224, 144), 1);
    text_center(center, sprite_y + sprite_size + (compact ? 21 : 27),
                pd_str(PD_STR_AMULET_LINE_2),
                rgb565(232, 230, 222), 1);
    ui_button(&layout->amulet_exit, pd_str(PD_STR_AMULET_EXIT));
    ui_button(&layout->amulet_stay, pd_str(PD_STR_AMULET_STAY));
}

static void draw_game_over(const pd_layout_t *layout) {
    const int banner_w = layout->width - layout->safe_left -
                         layout->safe_right - 20 < 128 ?
                         layout->width - layout->safe_left -
                         layout->safe_right - 20 : 128;
    const int banner_h = banner_w * 35 / 128;
    const int center_y = layout->safe_top +
        (layout->height - layout->safe_top - layout->safe_bottom) / 2;
    draw_shade(0, 0, layout->width, layout->height);
    ui_sprite(PD_UI_BANNER_GAME_OVER,
              (layout->width - banner_w) / 2,
              center_y - banner_h - 32, banner_w, banner_h);
    ui_button(&layout->menu_primary, pd_str(PD_STR_NEW_GAME));
    ui_button(&layout->menu_secondary, pd_str(PD_STR_BACK));
}

static void draw_title_background(const pd_layout_t *layout) {
    int width = layout->width;
    int height = width / 2;
    if (height < layout->height) {
        height = layout->height;
        width = height * 2;
    }
    fill_screen(layout, rgb565(12, 10, 8));
    sprite_use_slot(PD_TEXTURE_TITLE);
    sprite_instance((layout->width - width) / 2,
                    (layout->height - height) / 2, width, height,
                    0, 0, PD_TITLE_ATLAS_WIDTH, PD_TITLE_ATLAS_HEIGHT);
}

static void draw_title(const pd_game_t *game, const pd_layout_t *layout) {
    int banner_w = layout->width - layout->safe_left - layout->safe_right - 16;
    const int frame = (int)(g_now_ms / 42u % 24u);
    if (banner_w > 240) banner_w = 240;
    draw_title_background(layout);
    ui_sprite(PD_UI_BANNER_TITLE, layout->width / 2 - banner_w / 2,
              layout->safe_top + 10, banner_w, banner_w * 57 / 240);
    sprite_use_slot(PD_TEXTURE_FIRE);
    sprite_instance(layout->width / 2 - banner_w / 2 - 7,
                    layout->safe_top + 10, 42, 42,
                    (uint16_t)((frame % 8) * 24),
                    (uint16_t)((frame / 8) * 24), 24, 24);
    sprite_instance(layout->width / 2 + banner_w / 2 - 35,
                    layout->safe_top + 10, 42, 42,
                    (uint16_t)(((frame + 12) % 8) * 24),
                    (uint16_t)((frame + 12) / 8 * 24), 24, 24);
    ui_title_button(&layout->menu_primary, pd_str(PD_STR_ENTER_GAME),
                    PD_UI_TITLE_ENTER, 16, 16);
    ui_title_button(&layout->title_rankings, pd_str(PD_STR_RANKINGS),
                    PD_UI_TITLE_RANKINGS, 17, 16);
    ui_title_button(&layout->title_journal, pd_str(PD_STR_JOURNAL),
                    PD_UI_TITLE_JOURNAL, 17, 15);
    ui_title_button(&layout->title_settings, pd_str(PD_STR_SETTINGS),
                    PD_UI_PREFS, 14, 14);
    if (game->deepest > 1) {
        char line[32];
        pd_str_format(line, sizeof(line), PD_STR_BEST_DEPTH, game->deepest);
        text_center(layout->width / 2, layout->menu_secondary.y + 8, line,
                    rgb565(236, 218, 140), 1);
    }
}

static void draw_saves(const pd_game_t *game, const pd_layout_t *layout) {
    uint8_t slots[PD_SAVE_SLOTS];
    const int count = pd_game_visible_save_slots(game, slots);
    const int page_size = pd_layout_save_page_size(layout);
    int selected_index = 0;
    for (int index = 0; index < count; ++index)
        if (slots[index] == game->selected_slot) selected_index = index;
    const int start = pd_layout_save_page_start(layout, selected_index);
    const int visible = count - start < page_size ?
                        count - start : page_size;
    const pd_rect_t first = pd_layout_save_row(layout, visible, 0);
    int heading_y = layout->safe_top + 20;
    if (heading_y > first.y - 16) heading_y = first.y - 16;
    draw_title_background(layout);
    if (layout->display_shape != 2 || layout->width >= 220)
        text_center(layout->width / 2, heading_y,
                    pd_str(PD_STR_SAVED_GAME),
                    rgb565(250, 214, 96), 1);
    for (int index = 0; index < visible; ++index) {
        char label[32];
        const int slot_index = slots[start + index];
        const pd_save_slot_t *slot = &game->slots[slot_index];
        const pd_rect_t row = pd_layout_save_row(layout, visible, index);
        const pd_rect_t *rect = &row;
        ui_button(rect, "");
        if (slot->occupied) {
            char depth[32];
            char *out = append_text(label, pd_class_name(slot->cls));
            *out++ = ' ';
            out = append_int(out, slot_index + 1);
            *out = '\0';
            const int avatar_h = rect->h >= 40 ? 30 : 21;
            text_draw(rect->x + (rect->h >= 40 ? 43 : 32),
                      rect->y + (rect->h - 12) / 2, label,
                      rgb565(240, 224, 165), 1);
            pd_str_format(depth, sizeof(depth), PD_STR_STAT_DEPTH, slot->depth);
            text_right(rect->x + rect->w - 8,
                       rect->y + (rect->h - 12) / 2,
                       depth, rgb565(230, 230, 230), 1);
            ui_sprite((uint8_t)(PD_UI_AVATAR_WARRIOR + slot->cls),
                      rect->x + 9, rect->y + (rect->h - avatar_h) / 2,
                      rect->h >= 40 ? 22 : 15, avatar_h);
        } else {
            text_center(rect->x + rect->w / 2,
                       rect->y + (rect->h - 12) / 2,
                       pd_str(PD_STR_SAVE_EMPTY),
                       rgb565(165, 190, 165), 1);
        }
        if (game->selected_slot == slot_index)
            draw_rect(rect->x + 2, rect->y + (rect->h - 8) / 2, 3, 8,
                      rgb565(100, 235, 115));
    }
    if (count > page_size) {
        char label[12];
        char *out = append_int(label, start / page_size + 1);
        *out++ = '/';
        out = append_int(out, (count + page_size - 1) / page_size);
        *out++ = ' ';
        *out++ = '>';
        *out = '\0';
        const pd_rect_t button = pd_layout_save_page_button(layout, visible);
        ui_button(&button, label);
    }
    ui_scene_back(&layout->menu_back);
}

static void draw_rankings(const pd_game_t *game, const pd_layout_t *layout) {
    draw_title_background(layout);
    text_center(layout->width / 2, layout->safe_top + 20,
                pd_str(PD_STR_RANKINGS), rgb565(250, 214, 96), 1);
    if (!game->rankings[0].occupied)
        text_center(layout->width / 2, layout->height / 2,
                    pd_str(PD_STR_RANK_EMPTY), rgb565(185, 184, 175), 1);
    for (int index = 0; index < PD_RANK_COUNT; ++index) {
        char line[48];
        char *out = line;
        const pd_save_slot_t *slot = &game->rankings[index];
        if (!slot->occupied) continue;
        out = append_int(out, index + 1);
        out = append_text(out, ". ");
        out = append_text(out, pd_class_name(slot->cls));
        out = append_text(out, "  ");
        out = append_int(out, slot->deepest);
        *out = '\0';
        const pd_rect_t *row = &layout->save_row[0];
        const int row_y = row->y + index * 24;
        ui_ninepatch(PD_UI_BUTTON, row->x, row_y, row->w, 22, 4);
        text_center(layout->width / 2, row_y + 4,
                    line, rgb565(232, 228, 210), 1);
    }
    ui_scene_back(&layout->menu_back);
}

static void draw_journal(const pd_game_t *game, const pd_layout_t *layout) {
    const int width = layout->width - layout->safe_left - layout->safe_right - 12;
    const int left = (layout->width - width) / 2;
    const int top = layout->safe_top + 12;
    const int bottom = layout->height - layout->safe_bottom - 12;
    ui_ninepatch(PD_UI_WINDOW, left, top, width, bottom - top, 6);
    text_center(layout->width / 2, top + 12,
                pd_str(PD_STR_JOURNAL), rgb565(250, 214, 96), 1);
    int count = game->message_total < 6 ? game->message_total : 6;
    if (count == 0)
        text_center(layout->width / 2, top + 49,
                    pd_str(PD_STR_JOURNAL_EMPTY),
                    rgb565(180, 180, 175), 1);
    for (int index = 0; index < count; ++index) {
        const int message_index = (game->message_total - count + index) %
                                  PD_MESSAGE_COUNT;
        text_draw(left + 9, top + 31 + index * 20,
                  game->messages[message_index].text,
                  rgb565(222, 222, 216), 1);
    }
    ui_panel_close(&layout->journal_close);
}

static void draw_class_select(const pd_game_t *game, const pd_layout_t *layout) {
    static const pd_string_id_t kClassDescriptions[3] = {
        PD_STR_CLASS_1_DESC, PD_STR_CLASS_2_DESC, PD_STR_CLASS_3_DESC};
    char text[32];
    char *out;
    draw_title_background(layout);
    int heading_y = layout->safe_top + 30;
    if (heading_y > layout->class_button[0].y - 18)
        heading_y = layout->class_button[0].y - 18;
    if (layout->class_button[0].h > 32)
        text_center(layout->width / 2, heading_y,
                    pd_str(PD_STR_CHOOSE_HERO), rgb565(250, 214, 96), 1);
    for (int index = 0; index < PD_CLASS_COUNT; ++index) {
        const pd_rect_t *rect = &layout->class_button[index];
        ui_ninepatch(PD_UI_WINDOW, rect->x, rect->y, rect->w, rect->h, 6);
        if (rect->h < 60) {
            ui_sprite((uint8_t)(PD_UI_AVATAR_WARRIOR + index),
                      rect->x + 8, rect->y + 4,
                      rect->h < 40 ? 16 : 20,
                      rect->h < 40 ? 22 : 28);
            text_draw(rect->x + 37, rect->y + 3,
                      pd_class_name((uint8_t)index),
                      rgb565(240, 240, 244), 1);
        } else if (rect->h < 100) {
            ui_sprite((uint8_t)(PD_UI_AVATAR_WARRIOR + index),
                      rect->x + 10, rect->y + 15, 24, 32);
            text_draw(rect->x + 49, rect->y + 9,
                      pd_class_name((uint8_t)index),
                      rgb565(240, 240, 244), 1);
        } else {
            ui_sprite((uint8_t)(PD_UI_AVATAR_WARRIOR + index),
                      rect->x + rect->w / 2 - 12, rect->y + 5, 24, 32);
            text_center(rect->x + rect->w / 2, rect->y + 41,
                        pd_class_name((uint8_t)index),
                        rgb565(240, 240, 244), 1);
        }
        out = text;
        out = append_text(out, "STR ");
        out = append_int(out, index == 0 ? 11 : 10);
        *out = '\0';
        if (rect->h < 60) {
            text_draw(rect->x + 37, rect->y + 16, text,
                      rgb565(200, 200, 210), 1);
            if (rect->h >= 40 && rect->w >= 145)
                text_draw(rect->x + 83, rect->y + 16,
                          pd_str(kClassDescriptions[index]),
                          rgb565(150, 200, 240), 1);
        } else if (rect->h < 100) {
            text_draw(rect->x + 49, rect->y + 30, text,
                      rgb565(200, 200, 210), 1);
            text_draw(rect->x + 49, rect->y + rect->h - 22,
                      pd_str(kClassDescriptions[index]),
                      rgb565(150, 200, 240), 1);
        } else {
            text_center(rect->x + rect->w / 2, rect->y + 59, text,
                        rgb565(200, 200, 210), 1);
            text_center(rect->x + rect->w / 2, rect->y + 77,
                        pd_str(kClassDescriptions[index]),
                        rgb565(150, 200, 240), 1);
        }
    }
    out = text;
    out = append_text(out, pd_str(PD_STR_TAP_CLASS));
    *out = '\0';
    if (layout->display_shape != 2 || layout->height >= 220)
        text_center(layout->width / 2,
                    layout->display_shape == 2
                        ? layout->height - layout->safe_bottom - 12
                        : layout->class_button[PD_CLASS_COUNT - 1].y +
                          layout->class_button[PD_CLASS_COUNT - 1].h + 9,
                    text, rgb565(200, 200, 210), 1);
    ui_scene_back(&layout->menu_back);
    (void)game;
}

static void draw_bag(const pd_game_t *game, const pd_layout_t *layout) {
    char text[48];
    char *out;
    char detail[48];
    const int panel_x = layout->bag_row[0].x - 12;
    const int available_h = layout->height - layout->safe_top -
                            layout->safe_bottom - 8;
    const int max_panel_h = layout->bag_use[0].h > 32 ? 244 : 184;
    const int panel_h = available_h < max_panel_h ?
                        available_h : max_panel_h;
    const int panel_y = layout->safe_top +
        (layout->height - layout->safe_top - layout->safe_bottom - panel_h) / 2;
    const int panel_w = layout->width - layout->safe_left -
                        layout->safe_right < 250 ?
                        layout->width - layout->safe_left - layout->safe_right - 12 : 240;
    const int currency_right = layout->bag_close.x - 18;
    int gold_icon_x;
    draw_rect(panel_x - 2, panel_y - 2, panel_w + 4, panel_h + 4,
              rgb565(8, 8, 10));
    ui_ninepatch(PD_UI_WINDOW, panel_x, panel_y, panel_w, panel_h, 6);
    if (panel_w >= 180)
        text_draw(panel_x + 10, panel_y + 10,
                  pd_str(PD_STR_PACK_TITLE),
                  rgb565(250, 214, 96), 1);
    out = append_int(text, game->hero.gold);
    *out = '\0';
    gold_icon_x = currency_right - pd_font_width(text, 1) - 19;
    if (panel_w < 180) gold_icon_x = panel_x + 10;
    sprite_add(PD_SPRITE_ITEM_GOLD, gold_icon_x,
               panel_y + 8, 16, 16);
    const int gold_right = panel_w < 180 ?
                           gold_icon_x + 16 + pd_font_width(text, 1) :
                           currency_right;
    text_right(gold_right, panel_y + 11, text,
               rgb565(250, 214, 96), 1);
    if (game->hero.keys) {
        out = append_int(text, game->hero.keys);
        *out = '\0';
        if (panel_w >= 180) {
            const int key_right = gold_icon_x - 4;
            sprite_add(PD_SPRITE_ITEM_IRON_KEY,
                       key_right - pd_font_width(text, 1) - 19,
                       panel_y + 8, 16, 16);
            text_right(key_right, panel_y + 11, text,
                       rgb565(250, 214, 96), 1);
        } else {
            const int key_x = gold_right + 3;
            sprite_add(PD_SPRITE_ITEM_IRON_KEY,
                       key_x, panel_y + 8, 16, 16);
            text_draw(key_x + 16, panel_y + 11, text,
                      rgb565(250, 214, 96), 1);
        }
    }

    for (int index = 0; index < PD_BAG_ROWS; ++index) {
        const pd_rect_t *row = &layout->bag_row[index];
        ui_ninepatch(PD_UI_BUTTON, row->x, row->y, row->w, row->h, 4);
        if (index >= game->bag_count) continue;
        {
            const pd_item_t *item = &game->bag[index];
            const int equipped =
                index == game->hero.weapon || index == game->hero.armor;
            const int icon = row->h < 25 ? 16 :
                             (row->h >= 34 ? 24 : 18);
            sprite_add(pd_item_sprite(item), row->x + (row->w - icon) / 2,
                       row->y + (row->h - icon) / 2, icon, icon);
            if (equipped)
                text_draw(row->x + 3, row->y + 1, "E",
                          rgb565(250, 214, 96), 1);
            if (index == game->bag_selected)
            {
                const uint16_t edge = rgb565(250, 214, 96);
                draw_rect(row->x, row->y, row->w, 1, edge);
                draw_rect(row->x, row->y + row->h - 1, row->w, 1, edge);
                draw_rect(row->x, row->y, 1, row->h, edge);
                draw_rect(row->x + row->w - 1, row->y, 1, row->h, edge);
            }
        }
    }
    if (game->bag_count == 0) {
        text_center(panel_x + panel_w / 2, panel_y + 92,
                    pd_str(PD_STR_BAG_EMPTY),
                    rgb565(180, 180, 190), 1);
    } else {
        int selected = game->bag_selected;
        if (selected < 0 || selected >= game->bag_count) selected = 0;
        text_draw(panel_x + 12, layout->bag_use[0].y - 22,
                  pd_item_name(&game->bag[selected]),
                  rgb565(250, 214, 96), 1);
        pd_item_detail(&game->bag[selected], detail, sizeof(detail));
        text_right(panel_x + panel_w - 10, layout->bag_use[0].y - 22, detail,
                   rgb565(184, 203, 213), 1);
        ui_button(&layout->bag_use[0],
                  game->bag[selected].kind == PD_ITEM_AMULET ?
                  pd_str(PD_STR_AMULET_EXIT) :
                  game->bag[selected].kind == PD_ITEM_WEAPON ||
                  game->bag[selected].kind == PD_ITEM_ARMOR ?
                  pd_str(PD_STR_BAG_WIELD) : pd_str(PD_STR_BAG_USE));
        ui_button(&layout->bag_drop[0], pd_str(PD_STR_BAG_DROP));
    }
    ui_panel_close(&layout->bag_close);
}

static void draw_info(const pd_game_t *game, const pd_layout_t *layout) {
    const int available_w = layout->width - layout->safe_left -
                            layout->safe_right - 16;
    const int available_h = layout->height - layout->safe_top -
                            layout->safe_bottom - 12;
    const int width = available_w < 210 ? available_w : 210;
    const int height = available_h < 156 ? available_h : 156;
    const int left = (layout->width - width) / 2;
    const int top = (layout->height - height) / 2;
    char line[32];
    char *out;
    int y = top + 35;
    ui_ninepatch(PD_UI_WINDOW, left, top, width, height, 6);
    ui_panel_close(&layout->info_close);
    ui_sprite((uint8_t)(PD_UI_AVATAR_WARRIOR + game->hero.cls),
              left + 12, top + 9, 18, 24);
    text_draw(left + 36, top + 15, pd_str(PD_STR_PLAYER_INFO),
              rgb565(250, 214, 96), 1);
    for (int index = 0; index < 6; ++index) {
        const char *label;
        out = line;
        if (index == 0) {
            label = pd_str(PD_STR_PLAYER_STRENGTH);
            out = append_int(out, game->hero.str);
        } else if (index == 1) {
            label = pd_str(PD_STR_PLAYER_HEALTH);
            out = append_int(out, game->hero.hp);
            *out++ = '/';
            out = append_int(out, game->hero.max_hp);
        } else if (index == 2) {
            label = pd_str(PD_STR_PLAYER_EXPERIENCE);
            out = append_int(out, game->hero.xp);
            *out++ = '/';
            out = append_int(out, 6 + game->hero.level * 5);
        } else if (index == 3) {
            label = pd_str(PD_STR_PLAYER_SATIETY);
            out = append_int(out, game->hero.hunger);
            *out++ = '/';
            out = append_int(out, PD_HUNGER_MAX);
        } else if (index == 4) {
            label = pd_str(PD_STR_PLAYER_DEPTH);
            out = append_int(out, game->depth);
        } else {
            label = pd_str(PD_STR_PLAYER_GOLD);
            out = append_int(out, game->hero.gold);
        }
        *out = '\0';
        text_draw(left + 12, y, label, rgb565(236, 234, 218), 1);
        text_right(left + width - 12, y, line,
                   rgb565(226, 226, 226), 1);
        y += (height - 50) / 6;
    }
    if (game->hero.poison > 0 && height >= 150) {
        pd_str_format(line, sizeof(line), PD_STR_POISONED);
        text_draw(left + 12, y, line, rgb565(132, 210, 102), 1);
    }
}

static void draw_settings(const pd_layout_t *layout) {
    const int panel_w = layout->width - layout->safe_left -
                        layout->safe_right < 218 ?
                        layout->width - layout->safe_left -
                        layout->safe_right - 12 : 206;
    const int panel_h = 160;
    const int left = (layout->width - panel_w) / 2;
    const int top = (layout->height - panel_h) / 2;
    char zoom_text[8];
    char *end = append_int(zoom_text, layout->zoom);
    *end++ = 'x';
    *end = '\0';
    ui_ninepatch(PD_UI_WINDOW, left, top, panel_w, panel_h, 6);
    text_center(layout->width / 2, top + 13, pd_str(PD_STR_SETTINGS),
                rgb565(250, 214, 96), 1);
    text_center(layout->width / 2, top + 43, pd_str(PD_STR_MAP_ZOOM),
                rgb565(225, 222, 214), 1);
    ui_button(&layout->zoom_out, "-");
    text_center(layout->width / 2, layout->zoom_out.y + 9, zoom_text,
                rgb565(250, 214, 96), 1);
    ui_button(&layout->zoom_in, "+");
    ui_panel_close(&layout->settings_back);
    text_center(layout->width / 2, top + 115,
                pd_str(PD_STR_PINCH_HINT), rgb565(182, 184, 185), 1);
}

static void draw_pause(const pd_layout_t *layout) {
    const pd_rect_t *top = &layout->pause_settings;
    const int icon_size = top->h >= 40 ? 20 : 16;
    ui_ninepatch(PD_UI_WINDOW, top->x - 6, top->y - 6,
                 top->w + 12, layout->pause_menu.y +
                 layout->pause_menu.h - top->y + 12, 6);
    ui_red_button(&layout->pause_settings, pd_str(PD_STR_SETTINGS));
    ui_sprite(PD_UI_PREFS, top->x + 11,
              top->y + (top->h - icon_size) / 2,
              icon_size, icon_size);
    ui_red_button(&layout->pause_menu, pd_str(PD_STR_MAIN_MENU));
    ui_sprite(PD_UI_DISPLAY_ICON, layout->pause_menu.x + 11,
              layout->pause_menu.y +
              (layout->pause_menu.h - icon_size) / 2,
              icon_size, icon_size);
}

static void draw_shop(const pd_game_t *game, const pd_layout_t *layout) {
    const int width = layout->width - layout->safe_left -
                      layout->safe_right < 170 ?
                      layout->width - layout->safe_left -
                      layout->safe_right - 10 : 170;
    const int left = (layout->width - width) / 2;
    const int top = (layout->height - 140) / 2;
    char price[32];
    char gold[32];
    char *end;
    ui_ninepatch(PD_UI_WINDOW, left, top, width, 140, 6);
    text_center(layout->width / 2, top + 11, pd_str(PD_STR_SHOP_TITLE),
                rgb565(250, 214, 96), 1);
    sprite_add(PD_SPRITE_ITEM_POTION_HEAL, layout->width / 2 - 10,
               top + 28, 20, 20);
    end = append_int(price, 40 + game->depth * 4);
    *end = '\0';
    text_center(layout->width / 2, top + 53, price,
                rgb565(250, 214, 96), 1);
    end = append_int(gold, game->hero.gold);
    *end = '\0';
    text_center(layout->width / 2, top + 70, gold,
                rgb565(232, 226, 212), 1);
    ui_button(&layout->shop_buy, pd_str(PD_STR_SHOP_BUY_BUTTON));
    ui_panel_close(&layout->shop_close);
}

/* ---------------------------------------------------------------------- */
/* Entry point                                                             */
/* ---------------------------------------------------------------------- */

int pd_render_present(uint32_t context_handle, uint32_t capabilities,
                      const pd_game_t *game, const pd_layout_t *layout,
                      uint8_t *buffer, uint32_t capacity, uint64_t frame_id,
                      uint32_t now_ms) {
    int32_t status;
    int camera_x = 0;
    int camera_y = 0;
    g_capabilities = capabilities;
    g_now_ms = now_ms;
    g_sprite_count = 0;
    g_text_count = 0;
    g_tile_count = 0;
    g_wall_count = 0;
    g_overlay_count = 0;
    g_shade_count = 0;
    g_world_cell = layout->tile_pixels;
    pxa_raster_draw_list_begin(&g_list, buffer, capacity, frame_id);

    if (game->phase == PD_PHASE_TITLE || game->phase == PD_PHASE_RANKINGS ||
        (game->settings_from_title && (game->phase == PD_PHASE_SETTINGS ||
                                       game->phase == PD_PHASE_JOURNAL))) {
        if (game->phase == PD_PHASE_TITLE) draw_title(game, layout);
        else if (game->phase == PD_PHASE_RANKINGS) draw_rankings(game, layout);
        else {
            draw_title_background(layout);
            if (game->phase == PD_PHASE_SETTINGS) draw_settings(layout);
            else draw_journal(game, layout);
        }
        sprite_flush();
        text_flush();
        return pxa_raster_submit(context_handle, &g_list) > 0;
    }
    if (game->phase == PD_PHASE_SAVES || game->phase == PD_PHASE_CLASS) {
        if (game->phase == PD_PHASE_SAVES) draw_saves(game, layout);
        else draw_class_select(game, layout);
        sprite_flush();
        text_flush();
        return pxa_raster_submit(context_handle, &g_list) > 0;
    }
    if (game->phase == PD_PHASE_WON || game->phase == PD_PHASE_AMULET) {
        if (game->phase == PD_PHASE_WON)
            draw_victory_screen(game, layout);
        else
            draw_amulet_scene(layout);
        sprite_flush();
        text_flush();
        return pxa_raster_submit(context_handle, &g_list) > 0;
    }

    pd_layout_camera_visual_pixels(layout, game->hero.x, game->hero.y,
                                   game->hero_from_x, game->hero_from_y,
                                   game->hero_moving, &camera_x, &camera_y);
    (void)pxa_raster_clear(&g_list, rgb565(8, 8, 12));
#if !defined(PD_SKIP_MAP)
    draw_map(game, layout, camera_x, camera_y);
#endif
#if !defined(PD_SKIP_ENTITIES)
    draw_ground_items(game, layout, camera_x, camera_y);
    draw_item_sparkles(game, layout, camera_x, camera_y);
    draw_mobs(game, layout, camera_x, camera_y);
    draw_hero(game, layout, camera_x, camera_y);
#endif
    sprite_flush();
#if !defined(PD_SKIP_MAP)
    draw_upper_walls(game, layout, camera_x, camera_y);
    shade_flush();
#endif
#if !defined(PD_SKIP_ENTITIES)
    draw_effects(game, layout, camera_x, camera_y);
#endif
#if !defined(PD_SKIP_HUD)
    draw_hud(game, layout);
    draw_messages(game, layout);
    {
        const pd_rect_t *pane = &layout->menu_pane;
        const pd_rect_t *depth = &layout->depth;
        char floor[12];
        char *end = append_int(floor, game->depth);
        *end = '\0';
        const int art_scale = pane->w >= 93 && pane->h >= 42 ? 3 : 2;
        const int art_x = pane->x + (pane->w - 31 * art_scale) / 2;
        const int art_y = pane->y + (pane->h - 14 * art_scale) / 2;
        const int journal_x = art_x + 2 * art_scale;
        const int journal_y = art_y + art_scale;
        ui_sprite_without_header(PD_UI_MENU_PANE,
                                 art_x, art_y,
                                 31 * art_scale, 14 * art_scale);
        ui_sprite(PD_UI_MENU_JOURNAL,
                  journal_x, journal_y,
                  13 * art_scale, 11 * art_scale);
        ui_sprite(PD_UI_MENU_JOURNAL_ICON,
                  journal_x + art_scale,
                  journal_y + (11 - 6) * art_scale / 2,
                  11 * art_scale, 6 * art_scale);
        ui_sprite(PD_UI_MENU_BUTTON,
                  art_x + 17 * art_scale, journal_y,
                  12 * art_scale, 11 * art_scale);
        if (depth->w > 12) {
            ui_ninepatch(PD_UI_BUTTON, depth->x, depth->y,
                         depth->w, depth->h, 4);
            ui_sprite(PD_UI_DEPTH_ICON, depth->x + 9,
                      depth->y + 4, 12, 14);
            draw_hud_value_fit(depth->x + 3, depth->y + 30,
                               depth->w - 6, floor,
                               rgb565(202, 207, 194), 2);
        } else {
            ui_sprite(PD_UI_DEPTH_ICON, depth->x + 3,
                      depth->y + 2, 6, 7);
            draw_hud_value_fit(depth->x + 1, depth->y + 14,
                               depth->w - 2, floor,
                               rgb565(202, 207, 194), 1);
        }
    }
#endif
#if !defined(PD_SKIP_CONTROLS)
    draw_buttons(game, layout);
#endif
    if (game->phase == PD_PHASE_BAG) draw_bag(game, layout);
    if (game->phase == PD_PHASE_INFO) draw_info(game, layout);
    if (game->phase == PD_PHASE_JOURNAL) draw_journal(game, layout);
    if (game->phase == PD_PHASE_SETTINGS) draw_settings(layout);
    if (game->phase == PD_PHASE_PAUSE) draw_pause(layout);
    if (game->phase == PD_PHASE_SHOP) draw_shop(game, layout);
    if (game->phase == PD_PHASE_DEAD) draw_game_over(layout);
    sprite_flush();
    text_flush();

    status = pxa_raster_submit(context_handle, &g_list);
    if (status > 0) return 1;
    if (status != PXA_STATUS_WOULD_BLOCK) {
        char line[64];
        char *out = line;
        out = append_text(out, "pd: frame rejected status=");
        out = append_int(out, (int)status);
        *out = '\0';
        (void)pxa_log_error(line);
    }
    return status == PXA_STATUS_WOULD_BLOCK ? 1 : 0;
}
