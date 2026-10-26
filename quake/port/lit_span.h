/* PXA exact-pixel lit spans. GPL-2.0-or-later.
 * Lighting is immutable during a span. Reuse the interpolated vertical pair
 * while the texture row and horizontal light cell stay the same. */
#define defineCachedLitSpan(miplevel, power2)                                      \
__attribute__((noinline)) static void pxaDrawLitSpan##miplevel##_##power2(            \
    uint8_t *pdest, int count, int s, int t, int sstep, int tstep)                 \
{                                                                               \
    const int ds = sstep >> (16 - SPAN_PRECISION);                                \
    const int dt = tstep >> (16 - SPAN_PRECISION);                                \
    const int shift = mipShift(miplevel), mask = mipMask(miplevel);               \
    const int *blocklights = (const int *)sdd.blocklights;                        \
    const int lightwidth = sdd.r_lightwidth;                                      \
    const uint8_t *source = sdd.r_source, *colormap = sdd.colormap;                \
    const int width = sdd.r_texture_width, height = sdd.r_texture_height;          \
    const uint32_t qcs = sdd.qcs, qct = sdd.qct;                                  \
    const int lightMask = r_fullbright ? 0 : 0xFF00;                              \
    int s6 = s >> (16 - SPAN_PRECISION), t6 = t >> (16 - SPAN_PRECISION);         \
    /* XOR guarantees a cache miss for the first pixel, for any signed row. */  \
    int last_t = (((int16_t)t6) >> SPAN_PRECISION) ^ 1, last_cell = 0;            \
    int light_base = 0, light_step = 0;                                          \
    int last_s = INT32_MIN;                                                     \
    uint8_t last_pixel = 0;                                                     \
    uint8_t *end = pdest + count;                                                \
    do {                                                                        \
        int ss = (int16_t)s6, tt = (int16_t)t6;                                   \
        int si = ss >> SPAN_PRECISION, ti = tt >> SPAN_PRECISION;                 \
        /* Only power-of-two sampling ignores the fractional coordinate. */   \
        if (power2 && si == last_s && ti == last_t) {                            \
            *pdest++ = last_pixel;                                              \
            s6 += ds; t6 += dt;                                                 \
            continue;                                                          \
        }                                                                      \
        int cell = si >> shift;                                                 \
        if (ti != last_t || cell != last_cell) {                                 \
            const int *p = blocklights + cell + lightwidth * (ti >> shift);      \
            int l0 = p[0], l1 = p[1], dy = ti & mask;                            \
            int a0 = (int16_t)l0, a1 = (int16_t)l1;                              \
            int left = (a0 << shift) + ((l0 >> 16) - a0) * dy;                   \
            int right = (a1 << shift) + ((l1 >> 16) - a1) * dy;                  \
            light_base = left << shift;                                         \
            light_step = right - left;                                          \
            last_t = ti; last_cell = cell;                                      \
        }                                                                       \
        int light = light_base + light_step * (si & mask), x, y;                 \
        if (power2) {                                                           \
            y = ti & (height - 1); x = si & (width - 1);                          \
        } else {                                                                \
            y = ((uint64_t)(qct * ((uint32_t)tt << (16-SPAN_PRECISION)))          \
                 * height) >> 32;                                               \
            x = ((uint64_t)(qcs * ((uint32_t)ss << (16-SPAN_PRECISION)))          \
                 * width) >> 32;                                                \
        }                                                                       \
        last_pixel = colormap[((light >> shift) & lightMask)                    \
                               + source[y * width + x]];                        \
        *pdest++ = last_pixel; last_s = si;                                      \
        s6 += ds; t6 += dt;                                                     \
    } while (pdest < end);                                                      \
}

defineCachedLitSpan(0, 1)
defineCachedLitSpan(1, 1)
defineCachedLitSpan(2, 1)
defineCachedLitSpan(3, 1)
defineCachedLitSpan(0, 0)
defineCachedLitSpan(1, 0)
defineCachedLitSpan(2, 0)
defineCachedLitSpan(3, 0)

/* Keep selection in the existing dispatcher, so a rejected cache path does
 * not add another out-of-line renderer call. */
static inline __attribute__((always_inline)) int Pxa_TryCachedLitSpan(uint8_t *dest, int count, int s, int t,
                                     int sstep, int tstep)
{
    if (count < 8) return 0;
    const unsigned mip = r_drawsurf.surfmip & 3;
    int ds = sstep >> (16 - SPAN_PRECISION), dt = tstep >> (16 - SPAN_PRECISION);
    unsigned adt = dt < 0 ? -dt : dt, ads = ds < 0 ? -ds : ds;
    if (adt + (ads >> mipShift(mip)) > 16) return 0;
    if (!sdd.textureType) {
        switch (mip) {
        case 0: pxaDrawLitSpan0_1(dest,count,s,t,sstep,tstep); break;
        case 1: pxaDrawLitSpan1_1(dest,count,s,t,sstep,tstep); break;
        case 2: pxaDrawLitSpan2_1(dest,count,s,t,sstep,tstep); break;
        default: pxaDrawLitSpan3_1(dest,count,s,t,sstep,tstep); break;
        }
    } else {
        switch (mip) {
        case 0: pxaDrawLitSpan0_0(dest,count,s,t,sstep,tstep); break;
        case 1: pxaDrawLitSpan1_0(dest,count,s,t,sstep,tstep); break;
        case 2: pxaDrawLitSpan2_0(dest,count,s,t,sstep,tstep); break;
        default: pxaDrawLitSpan3_0(dest,count,s,t,sstep,tstep); break;
        }
    }
    return 1;
}
