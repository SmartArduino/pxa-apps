/* Cross-build pixel/depth probe: link unchanged against both renderers.
 * Covers horizontal/vertical flip, clipping, priority and unaligned rows. */
#include "core.h"
#include "vendor/snes9x/snes9x.h"
#include "vendor/snes9x/src/tile.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void DrawTile16(uint32_t, int32_t, uint32_t, uint32_t);
void DrawClippedTile16(uint32_t, int32_t, uint32_t, uint32_t, uint32_t, uint32_t);

static uint32_t random_value = 1;
static uint32_t next(void) {
    random_value = random_value * 1664525u + 1013904223u;
    return random_value;
}

int main(int argc, char **argv) {
    assert(argc == 2);
    FILE *file = fopen(argv[1], "rb");
    assert(file && !fseek(file, 0, SEEK_END));
    long length = ftell(file); assert(length > 0); rewind(file);
    uint8_t *rom = calloc(1, EMU_ROM_CAPACITY((size_t)length));
    assert(rom && fread(rom, 1, (size_t)length, file) == (size_t)length);
    fclose(file); assert(!emu_core_load(rom, (size_t)length));
    BG.Depth = 2; BG.BitShift = 8; BG.TileShift = 6;
    BG.TileAddress = BG.NameSelect = BG.StartPalette = 0;
    BG.PaletteMask = BG.PaletteShift = BG.DirectColourMode = 0;
    BG.Buffer = IPPU.TileCache; BG.Buffered = IPPU.TileCached;
    GFX.S = GFX.Screen; GFX.DB = GFX.ZBuffer; GFX.PPL = 512;
    static const uint8_t priorities[] = {0, 1, 2, 31, 127, 128, 254, 255};
    for (unsigned group = 0; group < 16; ++group) {
        uint32_t crc = 0;
        for (unsigned trial = 0; trial < 256; ++trial) {
            unsigned offset = 16 + (trial & 15);
            unsigned start = (trial >> 4) & 7;
            unsigned width = 1 + next() % (8 - start);
            unsigned row = next() % 8, count = 1 + next() % (8 - row);
            uint32_t flip = (group & 1 ? H_FLIP : 0) | (group & 2 ? V_FLIP : 0);
            GFX.Z1 = priorities[(trial >> 5) & 7];
            GFX.Z2 = (uint8_t)(next() >> 24);
            for (unsigned i = 0; i < 256; ++i)
                IPPU.ScreenColors[i] = (uint16_t)(next() >> 8);
            for (unsigned i = 0; i < 64; ++i)
                IPPU.TileCache[i] = next() & 1 ? 0 : (uint8_t)(next() >> 24);
            IPPU.TileCached[0] = TILE_CACHE_VALID | BG.Depth;
            for (unsigned i = 0; i < 512 * 8; ++i) {
                ((uint16_t *)GFX.Screen)[i] = (uint16_t)(next() >> 8);
                GFX.ZBuffer[i] = group & 8 ? GFX.Z2 : (uint8_t)(next() >> 24);
            }
            if (group & 4)
                DrawClippedTile16(flip, offset, start, width, row * 8, count);
            else
                DrawTile16(flip, offset, row * 8, count);
            crc = emu_port_crc32(crc, GFX.Screen, 512 * 8 * 2);
            crc = emu_port_crc32(crc, GFX.ZBuffer, 512 * 8);
            crc = emu_port_crc32(crc, IPPU.TileCache, 64);
        }
        printf("%02x %08x\n", group, crc);
    }
    emu_core_close(); free(rom);
    return 0;
}
