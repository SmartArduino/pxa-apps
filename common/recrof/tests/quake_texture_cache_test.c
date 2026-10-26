/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define MAX_TEXTURE_SIZE 32768
#define SKYSIZE 128
#define SORT_SURFS 1
#define NUMSTACKSURFACES 32
#define MSURFACE_HAS_TEXINFO_POINTER 1
#define DRAWSURF_SEC
#define EXT_MEM_BYTE_TIME 1
#define EXT_MEM_ACCESS_TIME 65
#define SURF_DRAWBACKGROUND 64
#define SURF_DRAWSKY 4
#define SURF_DRAWTURB 16
typedef uint8_t byte;
typedef struct { unsigned width, height; byte *extmemdata[4]; } texture_t;
typedef struct { texture_t *texture; } mtexinfo_t;
typedef struct { mtexinfo_t *texinfo; } msurface_t;
typedef struct { int frame; } entity_t;
typedef struct { msurface_t *data; int flags, miplevel, insubmodel; entity_t *entity; } surf_t;
static struct { surf_t surfaces[32]; entity_t *currententity; } state, *_g = &state;
uint16_t surfIdx[NUMSTACKSURFACES];
static int getSurfFlags(const surf_t *s) { return s->flags; }
static entity_t *getSurfEntity(const surf_t *s) { return s->entity; }
static texture_t *R_TextureAnimation(texture_t *t) { return t; }
static void fastMemclear32(void *p, unsigned n) { memset(p, 0, n * 4); }
static int extMemHasAsynchDataReadFinished(void) { return 1; }
static void extMemWaitAsynchDataRead(void) {}
static void extMemStopDMA(void) {}
static int extMemGetRemainingBytes(void) { return 0; }
static byte flash[8][32768];
static unsigned reads, sky_reads;
extern uint8_t textureCacheBuffer[MAX_TEXTURE_SIZE];
static void read_flash(uintptr_t addr, void *dest, unsigned size) {
    unsigned i = (unsigned)(addr / 65536u), offset = addr % 65536u;
    assert(i < 8 && size <= sizeof(flash[i]) - offset);
    assert((byte *)dest >= textureCacheBuffer);
    assert((byte *)dest + size <= textureCacheBuffer + MAX_TEXTURE_SIZE);
    memcpy(dest, flash[i] + offset, size);
    reads++;
    sky_reads += i == 1;
}
static void extMemStartAsynchDataRead(uint32_t addr, void *dest, unsigned size) {
    read_flash(addr, dest, size);
}
static void extMemGetDataFromAddress(void *dest, const void *addr, unsigned size) {
    read_flash((uintptr_t)addr, dest, size);
}
#include "texture_loader.h"
static texture_t textures[8];
static mtexinfo_t infos[8];
static msurface_t faces[8];
static void surface(unsigned slot, unsigned texture, int flags, unsigned mip) {
    state.surfaces[slot] = (surf_t){faces + texture, flags, (int)mip, 0, NULL};
    surfIdx[slot] = slot;
}
static void lit(unsigned slot, byte *last, unsigned total) {
    surf_t *s = state.surfaces + slot;
    texture_t *t = s->data->texinfo->texture;
    byte *address = t->extmemdata[s->miplevel];
    lightedSurfacePreTextureLoaderHandler(last, address, s->miplevel, slot, total);
    textureLoaderHandler(address, address, s->miplevel, slot, total);
    unsigned size = t->width * t->height >> (s->miplevel * 2);
    assert(!memcmp(getCurrentTextureCacheAddress(), flash[(uintptr_t)address / 65536], size));
}
int main(void) {
    for (unsigned i = 1; i < 8; ++i) {
        for (unsigned j = 0; j < sizeof(flash[i]); ++j) flash[i][j] = (byte)(i * 29 + j * 13);
        textures[i].width = 128; textures[i].height = 128;
        for (unsigned m = 0; m < 4; ++m) textures[i].extmemdata[m] = (byte *)(uintptr_t)(i * 65536);
        infos[i].texture = textures + i; faces[i].texinfo = infos + i;
    }
    /* A water surface prefetching across several sky faces must reach the wall.
     * Raw BSP sky pixels are not the animated/composited sky texture. */
    surface(0, 2, SURF_DRAWTURB, 0);
    surface(1, 1, SURF_DRAWSKY, 0); surface(2, 1, SURF_DRAWSKY, 0);
    surface(3, 3, 0, 0); surface(4, 4, 0, 1);
    clearTextureCache();
    textureLoaderHandler(NULL, textures[2].extmemdata[0], 0, 0, 5);
    lit(3, textures[2].extmemdata[0], 5);
    lit(4, textures[3].extmemdata[0], 5);
    assert(sky_reads == 0);
    /* Even an unexpected animation/prefetch identity must reload on non-WIN32. */
    for (unsigned state = TEXTURE_BUFFER_STATE_LOADING; state <= TEXTURE_BUFFER_STATE_READY; ++state) {
        clearTextureCache();
        textureCacheBuffer_t *other = &tcb[1 - currentTcbIndex];
        *other = (textureCacheBuffer_t){textures[5].extmemdata[0], 16384, 0, 0, state};
        read_flash((uintptr_t)other->extMemAddress, setTcbLoadParameters(other, TCB_LOAD_BLOCKING), 16384);
        other->bytesRead = state == TEXTURE_BUFFER_STATE_READY ? 16384 : 8192;
        other->bytesBeingRead = state == TEXTURE_BUFFER_STATE_READY ? 0 : 8192;
        lit(3, NULL, 5);
    }
    /* The unlit loader can also replace a stale, partially completed prefetch. */
    clearTextureCache();
    textureCacheBuffer_t *other = &tcb[1 - currentTcbIndex];
    *other = (textureCacheBuffer_t){textures[5].extmemdata[0], 16384, 0, 0, TEXTURE_BUFFER_STATE_LOADING};
    read_flash((uintptr_t)other->extMemAddress, setTcbLoadParameters(other, TCB_LOAD_BLOCKING), 16384);
    other->bytesRead = 8192; other->bytesBeingRead = 8192;
    textureLoaderHandler(NULL, textures[2].extmemdata[0], 0, 0, 5);
    assert(!memcmp(getCurrentTextureCacheAddress(), flash[2], 16384));
    /* Large textures cannot overlap either live slot; cover every mip and slot. */
    for (unsigned round = 0; round < 100; ++round) {
        clearTextureCache();
        textures[3].height = round & 1 ? 256 : 128;
        for (unsigned j = 0; j < 4; ++j) surface(j, 3 + j, 0, (round + j) % 4);
        byte *last = NULL;
        for (unsigned j = 0; j < 4; ++j) {
            lit(j, last, 4); last = textures[3 + j].extmemdata[(round + j) % 4];
        }
    }
    assert(reads > 100);
    puts("texture cache: sky transitions, stale prefetch, mips and bounds passed");
}
