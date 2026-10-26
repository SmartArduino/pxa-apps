#pragma once
#include <stdint.h>

#ifndef OL_SHADED_PALETTE_CACHE
#define OL_SHADED_PALETTE_CACHE 0
#endif

#ifndef OL_RENDER_PROFILE
#define OL_RENDER_PROFILE 0
#endif

// Expand a 240x180 or160x120 scene backwards in-place into320x240 RGB565.
void ol_classic_upscale(uint16_t *pixels,unsigned width);
#if OL_RENDER_PROFILE
struct OlRenderProfile {
    uint64_t transform_us, raster_us, clear_us;
    uint32_t draws, vertices, quads, triangles, clipped, spans, pixels, rejected;
};
extern OlRenderProfile ol_render_profile;
uint64_t ol_render_clock_us();
#endif

// RGB565 span with the original 16.16 UV/light interpolation and index-zero
// transparency. The calling engine owns clipping and every input array.
void ol_classic_span(uint16_t *output,int count,int32_t u,int32_t v,int32_t l,
    int32_t du,int32_t dv,int32_t dl,const uint8_t *tile,
    const uint8_t *lightmap,const uint16_t *palette,const uint16_t *shaded_palette=nullptr);

// Depth is projected screen-space Z in 15.16 fixed point. Transparent texels
// leave both buffers untouched; UI and translucent passes can disable writes.
void ol_classic_depth_span(uint16_t *output,uint16_t *depth,int count,int32_t z,
    int32_t dz,int32_t u,int32_t v,int32_t l,int32_t du,int32_t dv,int32_t dl,
    const uint8_t *tile,const uint8_t *lightmap,const uint16_t *palette,bool write,
    const uint16_t *shaded_palette=nullptr);

// Shared fixed-point layout for geometry and the batched PXA rasterizer.
    struct OlRasterVertex {
        int32_t x, y, z, w;
        int32_t u, v, l;

        inline OlRasterVertex operator + (const OlRasterVertex &p) const {
            OlRasterVertex ret;
            ret.x = x + p.x;
            ret.y = y;
            ret.z = z + p.z;
            ret.w = w + p.w;
            ret.u = u + p.u;
            ret.v = v + p.v;
            ret.l = l + p.l;
            return ret;
        }

        inline OlRasterVertex operator - (const OlRasterVertex &p) const {
            OlRasterVertex ret;
            ret.x = x - p.x;
            ret.y = y;
            ret.z = z - p.z;
            ret.w = w - p.w;
            ret.u = u - p.u;
            ret.v = v - p.v;
            ret.l = l - p.l;
            return ret;
        }

        inline OlRasterVertex operator * (const int32_t s) const {
            OlRasterVertex ret;
            ret.x = x * s;
            ret.y = y;
            ret.z = z * s;
            ret.w = w * s;
            ret.u = u * s;
            ret.v = v * s;
            ret.l = l * s;
            return ret;
        }

        inline OlRasterVertex operator / (const int32_t s) const {
            OlRasterVertex ret;
            ret.x = x / s;
            ret.y = y;
            ret.z = z / s;
            ret.w = w / s;
            ret.u = u / s;
            ret.v = v / s;
            ret.l = l / s;
            return ret;
        }
    };

struct OlRasterState {
    uint16_t *color, *depth;
    const uint8_t *tile, *lightmap;
    const uint16_t *palette;
    int32_t width, left, top, right, bottom;
    bool depth_test, depth_write;
    // Optional 32x256 RGB565 table, rebuilt whenever its lightmap or palette changes.
    const uint16_t *shaded_palette;
};
void ol_classic_draw(const OlRasterState &state, OlRasterVertex *vertices,
    const uint16_t *indices, const int32_t *quads, int quad_count,
    const int32_t *triangles, int triangle_count);
