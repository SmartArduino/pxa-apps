#include "block_textures.h"

#include "game.h"
#include "rc_math.h"

#define RGB565(r, g, b) \
    ((uint16_t)((((uint16_t)(r)&0xF8u) << 8) | (((uint16_t)(g)&0xFCu) << 3) | \
                ((uint16_t)(b) >> 3)))

#define TEXTURE_BLOCK_FIRST BLOCK_GRASS
#define TEXTURE_BLOCK_LAST BLOCK_CACTUS
#define TEXTURE_SAMPLE_COUNT \
    ((TEXTURE_BLOCK_LAST - TEXTURE_BLOCK_FIRST + 1) * 3 * 256)
#define TEXTURE_PALETTE_COLORS 256
#define TEXTURE_PALETTE_TRANSPARENT 0
#define TEXTURE_PALETTE_RESERVED_WHITE (TEXTURE_PALETTE_COLORS - 1)

static uint16_t g_texture[BLOCK_TYPE_COUNT][3][256];
static uint8_t g_index[BLOCK_TYPE_COUNT][3][256];
static uint16_t g_palette[TEXTURE_PALETTE_COLORS];
typedef struct {
    uint16_t color;
    uint16_t id;
} texture_sample_t;
static texture_sample_t g_samples[TEXTURE_SAMPLE_COUNT];
static uint8_t g_ready;

static inline uint32_t tex_hash(int x, int y, int seed) {
    uint32_t value = (uint32_t)x * 73856093u ^ (uint32_t)y * 19349663u ^
                     (uint32_t)seed * 83492791u;
    value = (value ^ (value >> 13)) * 1274126177u;
    return value ^ (value >> 16);
}

static void tex_pixel(int block, int kind, int x, int y, int r, int g, int b) {
    g_texture[block][kind][(y << 4) | x] = RGB565(r, g, b);
}

static void tex_fill(int block, int kind, int r, int g, int b, int amount,
                     int seed) {
    int x;
    int y;
    for (y = 0; y < 16; ++y) {
        for (x = 0; x < 16; ++x) {
            const int n =
                (int)(tex_hash(x, y, seed) % (uint32_t)(2 * amount + 1)) -
                amount;
            tex_pixel(block, kind, x, y, r + n, g + n, b + n);
        }
    }
}

static void build_textures(void) {
    int x;
    int y;
    /* Grass: green top, dirt bottom, dirt side with a jagged green fringe. */
    for (y = 0; y < 16; ++y) {
        for (x = 0; x < 16; ++x) {
            /* Soft 2x2 clumps over fine noise read more like turf than a
             * single high-frequency speckle. */
            const int clump = (int)(tex_hash(x >> 1, y >> 1, 16) % 19u) - 9;
            const int n = (int)(tex_hash(x, y, 17) % 9u) - 4;
            tex_pixel(BLOCK_GRASS, BLOCK_TEXTURE_TOP, x, y, 118 + clump + n,
                      178 + clump + n, 66 + clump + n);
        }
    }
    tex_fill(BLOCK_GRASS, BLOCK_TEXTURE_BOTTOM, 134, 96, 67, 12, 12);
    tex_fill(BLOCK_GRASS, BLOCK_TEXTURE_SIDE, 134, 96, 67, 12, 13);
    for (x = 0; x < 16; ++x) {
        const int fringe = 2 + (int)(tex_hash(x, 0, 14) % 3u);
        for (y = 0; y < fringe; ++y) {
            const int n = (int)(tex_hash(x, y, 15) % 25u) - 12;
            tex_pixel(BLOCK_GRASS, BLOCK_TEXTURE_SIDE, x, y, 96 + n,
                      158 + n, 56 + n);
        }
    }
    /* Dirt: brown noise with darker specks. */
    tex_fill(BLOCK_DIRT, BLOCK_TEXTURE_TOP, 134, 96, 67, 13, 21);
    tex_fill(BLOCK_DIRT, BLOCK_TEXTURE_SIDE, 134, 96, 67, 13, 21);
    tex_fill(BLOCK_DIRT, BLOCK_TEXTURE_BOTTOM, 134, 96, 67, 13, 21);
    for (y = 0; y < 16; ++y) {
        for (x = 0; x < 16; ++x) {
            if ((tex_hash(x, y, 22) & 31u) == 0u) {
                tex_pixel(BLOCK_DIRT, BLOCK_TEXTURE_SIDE, x, y, 106, 72, 48);
            }
        }
    }
    /* Stone: grey noise with a few dark cracks and light chips. */
    tex_fill(BLOCK_STONE, BLOCK_TEXTURE_TOP, 124, 124, 128, 14, 31);
    tex_fill(BLOCK_STONE, BLOCK_TEXTURE_SIDE, 124, 124, 128, 14, 31);
    tex_fill(BLOCK_STONE, BLOCK_TEXTURE_BOTTOM, 124, 124, 128, 14, 31);
    for (y = 0; y < 16; ++y) {
        for (x = 0; x < 16; ++x) {
            const uint32_t h = tex_hash(x, y, 32);
            if ((h & 63u) == 0u) {
                tex_pixel(BLOCK_STONE, BLOCK_TEXTURE_SIDE, x, y, 92, 92, 96);
            } else if ((h & 127u) == 1u) {
                tex_pixel(BLOCK_STONE, BLOCK_TEXTURE_SIDE, x, y, 148, 148,
                          152);
            }
        }
    }
    /* Sand: pale noise with faint darker grains. */
    tex_fill(BLOCK_SAND, BLOCK_TEXTURE_TOP, 218, 207, 160, 10, 41);
    tex_fill(BLOCK_SAND, BLOCK_TEXTURE_SIDE, 214, 202, 154, 10, 42);
    tex_fill(BLOCK_SAND, BLOCK_TEXTURE_BOTTOM, 210, 198, 150, 10, 43);
    for (y = 0; y < 16; ++y) {
        for (x = 0; x < 16; ++x) {
            if ((tex_hash(x, y, 44) & 15u) == 0u) {
                tex_pixel(BLOCK_SAND, BLOCK_TEXTURE_SIDE, x, y, 198, 184,
                          138);
            }
        }
    }
    /* Wood: oak bark with faint vertical grain on the sides, concentric
     * growth rings on the cut faces. */
    for (y = 0; y < 16; ++y) {
        for (x = 0; x < 16; ++x) {
            /* Side: vanilla oak bark is a flat base with low-contrast
             * vertical grain, one soft streak and a small knot. */
            const int column = (int)(tex_hash(x, 0, 53) % 9u) - 4;
            const int grain = (int)(tex_hash(x >> 1, y >> 2, 51) % 5u) - 2;
            const int streak = (x % 8) == 3 ? -9 : 0;
            int side = column + grain + streak;
            const int kx = x - 11;
            const int ky = y - 4;
            if (kx * kx + ky * ky <= 2) side -= 12;
            if (x == 0 || x == 15) side -= 5;
            tex_pixel(BLOCK_WOOD, BLOCK_TEXTURE_SIDE, x, y, 105 + side,
                      83 + side, 51 + side);
            /* Cut faces: bark rim, thin rings and a darker heart. */
            {
                const int dx = x - 7;
                const int dy = y - 7;
                const int radius =
                    (int)(rc_sqrt((float)(dx * dx + dy * dy)) + 0.5F);
                const int noise = (int)(tex_hash(x, y, 52) % 7u) - 3;
                int r;
                int g;
                int b;
                if (x == 0 || y == 0 || x == 15 || y == 15) {
                    r = 104 + noise;
                    g = 82 + noise;
                    b = 52 + noise;
                } else if (radius <= 1) {
                    r = 150 + noise;
                    g = 118 + noise;
                    b = 70 + noise;
                } else {
                    const int ring = (radius % 3) == 0 ? -26 : 0;
                    r = 172 + ring + noise;
                    g = 136 + ring + noise;
                    b = 84 + ring + noise;
                }
                tex_pixel(BLOCK_WOOD, BLOCK_TEXTURE_TOP, x, y, r, g, b);
                tex_pixel(BLOCK_WOOD, BLOCK_TEXTURE_BOTTOM, x, y, r, g, b);
            }
        }
    }
    /* Leaves: strong green noise with dark holes. */
    tex_fill(BLOCK_LEAVES, BLOCK_TEXTURE_TOP, 58, 143, 70, 30, 61);
    tex_fill(BLOCK_LEAVES, BLOCK_TEXTURE_SIDE, 54, 136, 66, 30, 62);
    tex_fill(BLOCK_LEAVES, BLOCK_TEXTURE_BOTTOM, 50, 128, 62, 30, 63);
    for (y = 0; y < 16; ++y) {
        for (x = 0; x < 16; ++x) {
            if ((tex_hash(x, y, 64) & 7u) == 0u) {
                tex_pixel(BLOCK_LEAVES, BLOCK_TEXTURE_SIDE, x, y, 34, 92, 44);
            }
        }
    }
    /* Water: two overlapping ripple lattices with a few sparkles. The ray
     * caster blends two scrolled samples of this tile, so the surface
     * shimmers without an obvious sliding band. */
    for (y = 0; y < 16; ++y) {
        for (x = 0; x < 16; ++x) {
            int wa = (x + (y >> 1)) & 7;
            int wb = (y - (x >> 1)) & 7;
            const int n = (int)(tex_hash(x, y, 71) % 9u) - 4;
            int bright;
            wa = wa < 4 ? wa : 7 - wa;
            wb = wb < 4 ? wb : 7 - wb;
            bright = 4 + (wa + wb) * 5 + n;
            if ((tex_hash(x, y, 72) & 63u) == 0u) {
                bright += 24;
            }
            tex_pixel(BLOCK_WATER, BLOCK_TEXTURE_TOP, x, y, 40 + bright / 2,
                      96 + bright, 200 + bright / 2);
            tex_pixel(BLOCK_WATER, BLOCK_TEXTURE_SIDE, x, y, 30 + bright / 3,
                      80 + bright * 3 / 4, 174 + bright / 2);
            tex_pixel(BLOCK_WATER, BLOCK_TEXTURE_BOTTOM, x, y, 22 + n, 60 + n,
                      148 + n);
        }
    }
    /* Planks: four-pixel boards with seams and grain. */
    for (y = 0; y < 16; ++y) {
        for (x = 0; x < 16; ++x) {
            const int seam = (y & 3) == 0 || (x & 7) == 0 ? -26 : 0;
            const int n = (int)(tex_hash(x, y, 81) % 19u) - 9;
            tex_pixel(BLOCK_PLANK, BLOCK_TEXTURE_TOP, x, y, 158 + seam + n,
                      128 + seam + n, 80 + seam + n);
            tex_pixel(BLOCK_PLANK, BLOCK_TEXTURE_SIDE, x, y, 158 + seam + n,
                      128 + seam + n, 80 + seam + n);
            tex_pixel(BLOCK_PLANK, BLOCK_TEXTURE_BOTTOM, x, y, 158 + seam + n,
                      128 + seam + n, 80 + seam + n);
        }
    }
    /* Brick: offset courses with light mortar. */
    for (y = 0; y < 16; ++y) {
        for (x = 0; x < 16; ++x) {
            const int course = y >> 2;
            const int shifted = (x + ((course & 1) != 0 ? 4 : 0)) & 15;
            const int mortar = (y & 3) == 0 || (shifted & 7) == 0;
            const int n = (int)(tex_hash(x, y, 91) % 17u) - 8;
            if (mortar) {
                tex_pixel(BLOCK_BRICK, BLOCK_TEXTURE_SIDE, x, y, 172 + n,
                          168 + n, 160 + n);
            } else {
                tex_pixel(BLOCK_BRICK, BLOCK_TEXTURE_SIDE, x, y, 152 + n,
                          72 + n, 56 + n);
            }
        }
    }
    tex_fill(BLOCK_BRICK, BLOCK_TEXTURE_TOP, 152, 72, 56, 14, 92);
    tex_fill(BLOCK_BRICK, BLOCK_TEXTURE_BOTTOM, 152, 72, 56, 14, 92);
    /* Glass: pale pane with a bright frame and a diagonal highlight. */
    for (y = 0; y < 16; ++y) {
        for (x = 0; x < 16; ++x) {
            const int frame = x == 0 || y == 0 || x == 15 || y == 15;
            const int shine = (x + y) == 6 || (x + y) == 7;
            if (frame) {
                tex_pixel(BLOCK_GLASS, BLOCK_TEXTURE_SIDE, x, y, 236, 248,
                          255);
            } else if (shine) {
                tex_pixel(BLOCK_GLASS, BLOCK_TEXTURE_SIDE, x, y, 226, 244,
                          252);
            } else {
                tex_pixel(BLOCK_GLASS, BLOCK_TEXTURE_SIDE, x, y, 176, 214,
                          230);
            }
            tex_pixel(BLOCK_GLASS, BLOCK_TEXTURE_TOP, x, y, 190, 226, 238);
            tex_pixel(BLOCK_GLASS, BLOCK_TEXTURE_BOTTOM, x, y, 170, 206, 222);
        }
    }
    /* Cobblestone: rounded stones over a dark base. */
    for (y = 0; y < 16; ++y) {
        for (x = 0; x < 16; ++x) {
            const int cell = (int)(tex_hash(x >> 2, y >> 2, 101) & 3u);
            const int base = 96 + cell * 14;
            const int n = (int)(tex_hash(x, y, 102) % 21u) - 10;
            const int edge = (x & 3) == 0 || (y & 3) == 0 ? -22 : 0;
            const int v = base + n + edge;
            tex_pixel(BLOCK_COBBLE, BLOCK_TEXTURE_SIDE, x, y, v, v, v);
            tex_pixel(BLOCK_COBBLE, BLOCK_TEXTURE_TOP, x, y, base + n,
                      base + n, base + n);
            tex_pixel(BLOCK_COBBLE, BLOCK_TEXTURE_BOTTOM, x, y, base + n,
                      base + n, base + n);
        }
    }
    /* Crafting table: planks with a 2x2 grid on top and a worn side. */
    for (y = 0; y < 16; ++y) {
        for (x = 0; x < 16; ++x) {
            const int grain = (int)(tex_hash(x, y, 121) % 17u) - 8;
            const int grid = (x & 7) == 0 || (y & 7) == 0;
            const int base = grid ? 120 : 168;
            tex_pixel(BLOCK_TABLE, BLOCK_TEXTURE_TOP, x, y, base + grain,
                      (base * 78 / 100) + grain, (base * 48 / 100) + grain);
            {
                const int band = y < 3 ? -28 : (y == 10 ? -34 : 0);
                const int b = 142 + band + grain;
                tex_pixel(BLOCK_TABLE, BLOCK_TEXTURE_SIDE, x, y, b,
                          (b * 78 / 100), (b * 50 / 100));
            }
            tex_pixel(BLOCK_TABLE, BLOCK_TEXTURE_BOTTOM, x, y, 140 + grain,
                      108 + grain, 68 + grain);
        }
    }
    /* Snow: bright with a faint blue cast. */
    tex_fill(BLOCK_SNOW, BLOCK_TEXTURE_TOP, 236, 240, 246, 6, 131);
    tex_fill(BLOCK_SNOW, BLOCK_TEXTURE_SIDE, 232, 238, 245, 6, 132);
    tex_fill(BLOCK_SNOW, BLOCK_TEXTURE_BOTTOM, 224, 230, 238, 6, 133);
    /* Gravel: mixed grey pebbles with dark pits. */
    for (y = 0; y < 16; ++y) {
        for (x = 0; x < 16; ++x) {
            const int n = (int)(tex_hash(x, y, 141) % 41u) - 20;
            const int base = 126 + n;
            if ((tex_hash(x, y, 142) & 31u) == 0u) {
                tex_pixel(BLOCK_GRAVEL, BLOCK_TEXTURE_TOP, x, y, 88, 84, 80);
            } else {
                tex_pixel(BLOCK_GRAVEL, BLOCK_TEXTURE_TOP, x, y, base,
                          base - 6, base - 14);
            }
            tex_pixel(BLOCK_GRAVEL, BLOCK_TEXTURE_SIDE, x, y, base - 6,
                      base - 12, base - 20);
            tex_pixel(BLOCK_GRAVEL, BLOCK_TEXTURE_BOTTOM, x, y, base - 12,
                      base - 18, base - 26);
        }
    }
    /* Cactus: green columns with ridges and pale spines. */
    for (y = 0; y < 16; ++y) {
        for (x = 0; x < 16; ++x) {
            const int ridge = (x % 5) == 0 || (x % 5) == 3 ? -18 : 6;
            const int n = (int)(tex_hash(x, y, 151) % 13u) - 6;
            int r = 52 + ridge + n;
            int g = 132 + ridge + n;
            int b = 56 + ridge + n;
            if ((tex_hash(x, y, 152) & 63u) == 0u) {
                r = 214;
                g = 226;
                b = 176;
            }
            tex_pixel(BLOCK_CACTUS, BLOCK_TEXTURE_SIDE, x, y, r, g, b);
            tex_pixel(BLOCK_CACTUS, BLOCK_TEXTURE_TOP, x, y, 62 + n,
                      146 + n, 66 + n);
            tex_pixel(BLOCK_CACTUS, BLOCK_TEXTURE_BOTTOM, x, y, 46 + n,
                      116 + n, 50 + n);
        }
    }
    /* Bush: dense dark foliage. */
    tex_fill(BLOCK_BUSH, BLOCK_TEXTURE_TOP, 44, 110, 50, 26, 161);
    tex_fill(BLOCK_BUSH, BLOCK_TEXTURE_SIDE, 38, 100, 44, 26, 162);
    tex_fill(BLOCK_BUSH, BLOCK_TEXTURE_BOTTOM, 32, 90, 38, 26, 163);
    /* Flower: grass with red and yellow blossoms. */
    tex_fill(BLOCK_FLOWER, BLOCK_TEXTURE_TOP, 70, 150, 60, 16, 171);
    tex_fill(BLOCK_FLOWER, BLOCK_TEXTURE_SIDE, 66, 142, 58, 16, 172);
    tex_fill(BLOCK_FLOWER, BLOCK_TEXTURE_BOTTOM, 60, 130, 54, 16, 173);
    for (y = 0; y < 16; ++y) {
        for (x = 0; x < 16; ++x) {
            const uint32_t h = tex_hash(x, y, 174);
            if ((h & 15u) == 0u) {
                const int red = (h & 16u) != 0u;
                tex_pixel(BLOCK_FLOWER, BLOCK_TEXTURE_TOP, x, y,
                          red ? 226 : 240, red ? 66 : 214, red ? 70 : 70);
                tex_pixel(BLOCK_FLOWER, BLOCK_TEXTURE_SIDE, x, y,
                          red ? 226 : 240, red ? 66 : 214, red ? 70 : 70);
            }
        }
    }
    /* Wool: soft off-white weave. */
    for (y = 0; y < 16; ++y) {
        for (x = 0; x < 16; ++x) {
            const int weave = ((x + y) & 3) == 0 ? -10 : 4;
            const int n = (int)(tex_hash(x, y, 181) % 11u) - 5;
            tex_pixel(BLOCK_WOOL, BLOCK_TEXTURE_TOP, x, y, 232 + weave + n,
                      230 + weave + n, 220 + weave + n);
            tex_pixel(BLOCK_WOOL, BLOCK_TEXTURE_SIDE, x, y, 226 + weave + n,
                      224 + weave + n, 214 + weave + n);
            tex_pixel(BLOCK_WOOL, BLOCK_TEXTURE_BOTTOM, x, y, 218 + weave + n,
                      216 + weave + n, 206 + weave + n);
        }
    }
    /* Bedrock: large dark blotches. */
    for (y = 0; y < 16; ++y) {
        for (x = 0; x < 16; ++x) {
            const int blotch =
                (int)(tex_hash(x >> 2, y >> 2, 111) % 41u) - 20;
            const int n = (int)(tex_hash(x, y, 112) % 17u) - 8;
            const int v = 58 + blotch + n;
            tex_pixel(BLOCK_BEDROCK, BLOCK_TEXTURE_TOP, x, y, v, v, v);
            tex_pixel(BLOCK_BEDROCK, BLOCK_TEXTURE_SIDE, x, y, v, v, v);
            tex_pixel(BLOCK_BEDROCK, BLOCK_TEXTURE_BOTTOM, x, y, v, v, v);
        }
    }
}

static int color_channel(uint16_t color, int channel) {
    if (channel == 0) return (color >> 11) & 31;
    if (channel == 1) return (color >> 5) & 63;
    return color & 31;
}

typedef struct {
    uint16_t begin;
    uint16_t end;
    uint8_t rmin;
    uint8_t rmax;
    uint8_t gmin;
    uint8_t gmax;
    uint8_t bmin;
    uint8_t bmax;
    uint8_t channel;
} texture_box_t;

/* Counting median split: no sorting and no allocation. The median element is
 * located with a channel histogram and a three-way in-place partition. */
static void swap_samples(uint16_t left, uint16_t right) {
    const texture_sample_t value = g_samples[left];
    g_samples[left] = g_samples[right];
    g_samples[right] = value;
}

static void split_samples(texture_box_t *box, texture_box_t *right) {
    const int channel = box->channel;
    const uint16_t begin = box->begin;
    const uint16_t end = box->end;
    const uint16_t half = (uint16_t)((end - begin) / 2u);
    uint16_t counts[64];
    uint8_t pivot_bin = 0;
    uint32_t accumulated = 0;
    uint16_t index;
    int bin;
    for (bin = 0; bin < 64; ++bin) counts[bin] = 0;
    for (index = begin; index < end; ++index)
        ++counts[color_channel(g_samples[index].color, channel)];
    for (bin = 0; bin < 64; ++bin) {
        if (accumulated + counts[bin] > half) break;
        accumulated += counts[bin];
    }
    pivot_bin = (uint8_t)bin;
    {
        uint16_t low = begin;
        uint16_t high = end;
        index = begin;
        while (index < high) {
            const int value = color_channel(g_samples[index].color, channel);
            if (value < pivot_bin) {
                if (index != low) swap_samples(index, low);
                ++low;
                ++index;
            } else if (value > pivot_bin) {
                --high;
                swap_samples(index, high);
            } else {
                ++index;
            }
        }
        box->end = (uint16_t)(begin + half);
        right->begin = box->end;
    }
}

static void box_measure(texture_box_t *box) {
    int rmin = 31;
    int rmax = 0;
    int gmin = 63;
    int gmax = 0;
    int bmin = 31;
    int bmax = 0;
    int rr;
    int gr;
    int br;
    uint16_t index;
    for (index = box->begin; index < box->end; ++index) {
        const uint16_t color = g_samples[index].color;
        const int r = color_channel(color, 0);
        const int g = color_channel(color, 1);
        const int b = color_channel(color, 2);
        if (r < rmin) rmin = r;
        if (r > rmax) rmax = r;
        if (g < gmin) gmin = g;
        if (g > gmax) gmax = g;
        if (b < bmin) bmin = b;
        if (b > bmax) bmax = b;
    }
    box->rmin = (uint8_t)rmin;
    box->rmax = (uint8_t)rmax;
    box->gmin = (uint8_t)gmin;
    box->gmax = (uint8_t)gmax;
    box->bmin = (uint8_t)bmin;
    box->bmax = (uint8_t)bmax;
    /* Normalise the channel ranges to 8 bits before comparing them. */
    rr = (rmax - rmin) * 8;
    gr = (gmax - gmin) * 4;
    br = (bmax - bmin) * 8;
    box->channel = rr >= gr ? (rr >= br ? 0u : 2u)
                            : (gr >= br ? 1u : 2u);
}

static int box_range(const texture_box_t *box) {
    const int rr = (box->rmax - box->rmin) * 8;
    const int gr = (box->gmax - box->gmin) * 4;
    const int br = (box->bmax - box->bmin) * 8;
    return rr > gr ? (rr > br ? rr : br) : (gr > br ? gr : br);
}

static void build_palette(void) {
    texture_box_t boxes[TEXTURE_PALETTE_COLORS];
    uint16_t box_count = 1;
    uint16_t index;
    for (index = 0; index < TEXTURE_SAMPLE_COUNT; ++index) {
        const uint16_t id = index;
        const int block = (int)(id / 768u) + TEXTURE_BLOCK_FIRST;
        const int kind = (int)((id / 256u) % 3u);
        g_samples[index].color = g_texture[block][kind][id & 255u];
        g_samples[index].id = id;
    }
    boxes[0].begin = 0;
    boxes[0].end = TEXTURE_SAMPLE_COUNT;
    box_measure(&boxes[0]);
    /* Index 0 is reserved for cutout texels and index 255 for the HUD font. */
    while (box_count < TEXTURE_PALETTE_RESERVED_WHITE - 1u) {
        int best = -1;
        int best_range = 0;
        texture_box_t *box;
        texture_box_t right;
        for (index = 0; index < box_count; ++index) {
            const int range = box_range(&boxes[index]);
            if (boxes[index].end - boxes[index].begin >= 2 &&
                range > best_range) {
                best_range = range;
                best = (int)index;
            }
        }
        if (best < 0 || best_range == 0) break;
        box = &boxes[best];
        right = *box;
        split_samples(box, &right);
        box_measure(box);
        box_measure(&right);
        boxes[box_count++] = right;
    }
    for (index = 0; index < box_count; ++index) {
        uint32_t red = 0;
        uint32_t green = 0;
        uint32_t blue = 0;
        uint32_t count = boxes[index].end - boxes[index].begin;
        uint16_t sample;
        if (count == 0) count = 1;
        for (sample = boxes[index].begin; sample < boxes[index].end; ++sample) {
            const uint16_t color = g_samples[sample].color;
            red += (uint32_t)((color >> 11) & 31u);
            green += (uint32_t)((color >> 5) & 63u);
            blue += (uint32_t)(color & 31u);
        }
        const uint16_t palette_index = (uint16_t)(index + 1u);
        g_palette[palette_index] = RGB565(
            (uint8_t)(red * 255u / (31u * count)),
            (uint8_t)(green * 255u / (63u * count)),
            (uint8_t)(blue * 255u / (31u * count)));
        for (sample = boxes[index].begin; sample < boxes[index].end;
             ++sample) {
            const uint16_t id = g_samples[sample].id;
            const int block = (int)(id / 768u) + TEXTURE_BLOCK_FIRST;
            const int kind = (int)((id / 256u) % 3u);
            g_index[block][kind][id & 255u] = (uint8_t)palette_index;
        }
    }
    while (index < TEXTURE_PALETTE_RESERVED_WHITE - 1u) {
        g_palette[index + 1u] = g_palette[box_count];
        ++index;
    }
    g_palette[TEXTURE_PALETTE_TRANSPARENT] = 0;
    /* The HUD font texture uses index 255 and expects white. */
    g_palette[TEXTURE_PALETTE_RESERVED_WHITE] = UINT16_C(0xffff);
}

static void apply_cutout_masks(void) {
    int kind;
    int x;
    int y;
    for (kind = 0; kind < 3; ++kind) {
        for (y = 0; y < 16; ++y) {
            for (x = 0; x < 16; ++x) {
                const int offset = (y << 4) | x;
                /* Sparse, stable holes preserve the leafy silhouette without
                 * alpha blending or a second transparent pass. */
                if ((tex_hash(x, y, 200 + kind) & 3u) == 0u)
                    g_index[BLOCK_LEAVES][kind][offset] =
                        TEXTURE_PALETTE_TRANSPARENT;
            }
        }
    }
}

static void build_once(void) {
    if (g_ready) return;
    build_textures();
    build_palette();
    apply_cutout_masks();
    g_ready = 1;
}

const block_texture_set_t *block_textures(void) {
    build_once();
    return (const block_texture_set_t *)g_texture;
}

const uint16_t *block_texture_palette(void) {
    build_once();
    return g_palette;
}

const block_index_set_t *block_texture_indices(void) {
    build_once();
    return (const block_index_set_t *)g_index;
}
