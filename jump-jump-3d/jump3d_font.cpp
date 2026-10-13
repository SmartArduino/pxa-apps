#include "jump3d_font.hpp"

#include <stddef.h>

#if J3_EMBED_FONTS
#include "jump3d_font_data.hpp"
#else
#include "jump3d_font_metrics.hpp"
#endif

namespace jump {

/* Antialiased coverage paths can be compiled out for measurements: 0 forces the
 * binary cut-out so builds with and without AA can be compared on one Host. */
#ifndef J3_FONT_AA
#define J3_FONT_AA 1
#endif

static_assert(J3_FONT_MAX_ATLAS_BYTES_DATA <= J3_FONT_MAX_ATLAS_BYTES,
               "raise J3_FONT_MAX_ATLAS_BYTES: a glyph atlas no longer fits "
               "the renderer's upload scratch");

static uint8_t g_tier;

static const j3_font_face_t *face_for(uint8_t font) {
    uint8_t tier = g_tier;
    if (tier >= J3_FONT_TIERS) tier = J3_FONT_TIERS - 1u;
    switch (font) {
    case J3_FONT_BIG: return &j3_font_big_faces[tier];
    case J3_FONT_SMALL: return &j3_font_small_faces[tier];
    case J3_FONT_CJK: return &j3_font_cjk_faces[tier];
    default: return &j3_font_big_faces[tier];
    }
}

void j3_font_set_tier(uint8_t tier) {
    g_tier = tier < J3_FONT_TIERS ? tier : (uint8_t)(J3_FONT_TIERS - 1u);
}

uint8_t j3_font_tier(void) { return g_tier; }

const j3_font_face_t& j3_font_face(uint8_t font) { return *face_for(font); }

const char* j3_font_asset_path(uint8_t font) {
    static constexpr const char* paths[3][J3_FONT_FACES]{
        {"assets/fonts/j3_font_big_t0.pxr","assets/fonts/j3_font_small_t0.pxr","assets/fonts/j3_font_cjk_t0.pxr"},
        {"assets/fonts/j3_font_big_t1.pxr","assets/fonts/j3_font_small_t1.pxr","assets/fonts/j3_font_cjk_t1.pxr"},
        {"assets/fonts/j3_font_big_t2.pxr","assets/fonts/j3_font_small_t2.pxr","assets/fonts/j3_font_cjk_t2.pxr"}};
    return paths[g_tier][font<J3_FONT_FACES?font:J3_FONT_BIG];
}

static int bytes_equal(const char *left, const char *right, int length) {
    int index;
    for (index = 0; index < length; ++index)
        if (left[index] != right[index]) return 0;
    return 1;
}

/* Returns the UTF-8 length of the character starting at `text`, or 0 when the
 * sequence is malformed. */
static int utf8_length(const char *text) {
    const uint8_t first = (uint8_t)text[0];
    if (first == 0) return 0;
    if (first < 0x80u) return 1;
    if ((first & 0xE0u) == 0xC0u && text[1] != '\0') return 2;
    if ((first & 0xF0u) == 0xE0u && text[1] != '\0' && text[2] != '\0')
        return 3;
    if ((first & 0xF8u) == 0xF0u && text[1] != '\0' && text[2] != '\0' &&
        text[3] != '\0')
        return 4;
    return 0;
}

/* Glyph cell index for the character at `text`, or -1 when it is not in the
 * atlas (the caller advances by the UTF-8 length either way). */
static int glyph_index(const j3_font_face_t *face, const char *text) {
    int position = 0;
    const int length = utf8_length(text);
    if (length == 0) return -1;
    while (face->glyphs[position] != '\0') {
        const int candidate = utf8_length(&face->glyphs[position]);
        if (candidate == 0) break;
        if (candidate == length &&
            bytes_equal(&face->glyphs[position], text, length))
            return position / length;
        position += candidate;
    }
    return -1;
}

int j3_font_upload(pxa::game::Renderer& context, uint8_t *scratch,
                   uint32_t scratch_capacity) {
    const uint8_t tier = j3_font_tier();
    if (scratch == NULL) return 0;
    {
        const j3_font_face_t *faces[J3_FONT_FACES];
        uint8_t font;
        faces[J3_FONT_BIG] = &j3_font_big_faces[tier];
        faces[J3_FONT_SMALL] = &j3_font_small_faces[tier];
        faces[J3_FONT_CJK] = &j3_font_cjk_faces[tier];
        for (font = 0; font < J3_FONT_FACES; ++font) {
            const j3_font_face_t *face = faces[font];
            const uint32_t payload =
                (uint32_t)face->atlas_width * face->atlas_height;
            const uint8_t slot =
                (uint8_t)(J3_FONT_SLOT_BASE + font);
            if ((uint32_t)(pxa::game::Upload::header_bytes + payload) >
                scratch_capacity)
                return 0;
            {
                const int32_t result = arcade::upload_texture_index8(
                    context, slot, face->atlas_width, face->atlas_height,
                    face->pixels, scratch, scratch_capacity);
                if (result != (int32_t)(pxa::game::Upload::header_bytes + payload)) {
                    char text[64];
                    int length = 0;
                    static const char prefix[] = "font upload failed slot=";
                    while (prefix[length] != '\0') {
                        text[length] = prefix[length];
                        ++length;
                    }
                    text[length++] = (char)('0' + (slot / 10u) % 10u);
                    text[length++] = (char)('0' + slot % 10u);
                    text[length++] = ' ';
                    text[length++] = (char)('0' + (int)(result / 100) % 10);
                    text[length++] = (char)('0' + (int)(result / 10) % 10);
                    text[length++] = (char)('0' + (int)(result % 10));
                    text[length] = '\0';
                    (void)arcade::log(4, text);
                    return 0;
                }
            }
        }
    }
    return 1;
}

float j3_font_cell_height(uint8_t font) {
    return (float)face_for(font)->cell_height;
}

int j3_font_width(uint8_t font, int scale, const char *text) {
    const j3_font_face_t *face = face_for(font);
    int length = 0;
    int position = 0;
    if (text == NULL) return 0;
    while (text[position] != '\0') {
        const int size = utf8_length(&text[position]);
        if (size == 0) break;
        position += size;
        ++length;
    }
    return length * (int)face->cell_width * scale;
}

void j3_font_draw(arcade::DrawList *list, uint32_t capabilities,
                  uint8_t font, int x, int y, int scale, const char *text,
                  uint16_t color, uint8_t mode, uint8_t ramp_base) {
    const j3_font_face_t *face = face_for(font);
    const uint8_t slot =
        (uint8_t)(J3_FONT_SLOT_BASE + font);
    uint8_t flags = arcade::sprite_transparent_index0;
    uint16_t sprite_color = color;
    int position = 0;
    if (list == NULL || text == NULL || scale <= 0) return;
#if J3_FONT_AA
    if (mode == J3_FONT_RAMP &&
        (capabilities & pxa::game::capability_bit(pxa::game::RenderCapability::sprite_palette_ramp)) != 0u) {
        /* The atlas texel is coverage; the palette block holds the ink already
         * blended with the background, so this is exact and costs no more than
         * the palette lookup the sprite path already does. */
        flags |= arcade::sprite_palette_ramp;
        sprite_color = ramp_base;
    } else if (mode == J3_FONT_ALPHA &&
               (capabilities & pxa::game::capability_bit(pxa::game::RenderCapability::sprite_texel_alpha)) != 0u) {
        /* Coverage blended against whatever is behind the glyph. */
        flags |= arcade::sprite_solid_color |
                 arcade::sprite_texel_alpha;
    } else
#endif
    {
        (void)mode;
        (void)ramp_base;
        flags |= arcade::sprite_solid_color;
    }
    while (text[position] != '\0') {
        const int length = utf8_length(&text[position]);
        int cell;
        if (length == 0) break;
        cell = glyph_index(face, &text[position]);
        position += length;
        if (cell < 0) {
            x += (int)face->cell_width * scale;
            continue;
        }
        (void)arcade::sprite(
            list, slot, flags, capabilities, (int16_t)x, (int16_t)y,
            (uint16_t)(face->cell_width * scale),
            (uint16_t)(face->cell_height * scale),
            (uint16_t)((cell % (int)face->columns) * face->cell_width),
            (uint16_t)((cell / (int)face->columns) * face->cell_height),
            face->cell_width, face->cell_height, sprite_color);
        x += (int)face->cell_width * scale;
    }
}

} // namespace jump
