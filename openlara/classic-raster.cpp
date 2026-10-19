#include "classic-raster.h"

#if OL_RENDER_PROFILE
#include <time.h>
OlRenderProfile ol_render_profile;
uint64_t ol_render_clock_us() {
    timespec t{};clock_gettime(CLOCK_MONOTONIC,&t);
    return uint64_t(t.tv_sec)*1000000u+t.tv_nsec/1000u;
}
#endif

void ol_classic_upscale(uint16_t *pixels,unsigned width) {
    if(width!=240 && width!=160)return;
    // Every source index is <= its destination index, so backwards traversal
    // preserves unread pixels without another frame buffer or heap allocation.
    for(int y=239;y>=0;--y) {
        auto *dest=pixels+y*320;
        const auto *source=pixels+(width==240?((y*3)>>2)*240:(y>>1)*160);
        for(int x=319;x>=0;--x)dest[x]=source[width==240?(x*3)>>2:x>>1];
    }
}

static inline __attribute__((always_inline)) void color_span(uint16_t *output,int count,int32_t u,int32_t v,int32_t l,
    int32_t du,int32_t dv,int32_t dl,const uint8_t *tile,
    const uint8_t *lightmap,const uint16_t *palette) {
    auto pixel=[&]() {
        unsigned index=tile[((uint32_t(v)>>16)<<8)+(uint32_t(u)>>16)];
        if(index) *output=palette[lightmap[((l>>19)<<8)+index]];
        ++output;u+=du;v+=dv;l+=dl;
    };
    while(count>=4) {pixel();pixel();pixel();pixel();count-=4;}
    while(count-->0)pixel();
}

void ol_classic_span(uint16_t *output,int count,int32_t u,int32_t v,int32_t l,
    int32_t du,int32_t dv,int32_t dl,const uint8_t *tile,
    const uint8_t *lightmap,const uint16_t *palette) {
    color_span(output,count,u,v,l,du,dv,dl,tile,lightmap,palette);
}

template<bool Write>
static inline __attribute__((always_inline)) void depth_span(uint16_t *output,uint16_t *depth,int count,int32_t z,
    int32_t dz,int32_t u,int32_t v,int32_t l,int32_t du,int32_t dv,int32_t dl,
    const uint8_t *tile,const uint8_t *lightmap,const uint16_t *palette) {
    auto pixel=[&]() {
        const uint16_t value=uint32_t(z)>>15;
        if(value<=*depth) {
            unsigned index=tile[((uint32_t(v)>>16)<<8)+(uint32_t(u)>>16)];
            if(index) {
                *output=palette[lightmap[((l>>19)<<8)+index]];
                if(Write)*depth=value;
            }
        }
        ++output;++depth;z+=dz;u+=du;v+=dv;l+=dl;
    };
    while(count>=4) {pixel();pixel();pixel();pixel();count-=4;}
    while(count-->0)pixel();
}

void ol_classic_depth_span(uint16_t *output,uint16_t *depth,int count,int32_t z,
    int32_t dz,int32_t u,int32_t v,int32_t l,int32_t du,int32_t dv,int32_t dl,
    const uint8_t *tile,const uint8_t *lightmap,const uint16_t *palette,bool write) {
    if(write)depth_span<true>(output,depth,count,z,dz,u,v,l,du,dv,dl,tile,lightmap,palette);
    else depth_span<false>(output,depth,count,z,dz,u,v,l,du,dv,dl,tile,lightmap,palette);
}

namespace {
// Keep edge setup and pixel kernels in one O3 translation unit so that a span
// does not cross a Wasm function/stack-check boundary for every scanline.
struct RasterBatch {
    using VertexSW = OlRasterVertex;
    using Index = uint16_t;
    using int32 = int32_t;
    const OlRasterState &state;
    VertexSW *vertices;
    RasterBatch(const OlRasterState &s,VertexSW *v):state(s),vertices(v) {}
    template<class T> static void swap(T &a,T &b) {T tmp=a;a=b;b=tmp;}
    bool checkBackface(const VertexSW *a, const VertexSW *b, const VertexSW *c) {
        return ((b->x - a->x) >> 16) * (c->y - a->y) -
               ((c->x - a->x) >> 16) * (b->y - a->y) <= 0;
    }

    inline void sortVertices(VertexSW *&t, VertexSW *&m, VertexSW *&b) {
        if (t->y > m->y) swap(t, m);
        if (t->y > b->y) swap(t, b);
        if (m->y > b->y) swap(m, b);
    }

    inline void sortVertices(VertexSW *&t, VertexSW *&m, VertexSW *&b, VertexSW *&o) {
        if (t->y > m->y) swap(t, m);
        if (o->y > b->y) swap(o, b);
        if (t->y > o->y) swap(t, o);
        if (m->y > b->y) swap(m, b);
        if (m->y > o->y) swap(m, o);
    }

    inline void step(VertexSW &v, const VertexSW &d) {
        //v.w += d.w;
        v.u += d.u;
        v.v += d.v;
        v.l += d.l;
    }

    inline void step(VertexSW &v, const VertexSW &d, int32 count) {
        //v.w += d.w * count;
        v.u += d.u * count;
        v.v += d.v * count;
        v.l += d.l * count;
    }

    void drawLine(const VertexSW &L, const VertexSW &R, int32 y) {
        int32 x1 = L.x >> 16;
        int32 x2 = R.x >> 16;

        int32 f = x2 - x1;
        if (f <= 0) return;

        VertexSW dS = (R - L) / f;
        VertexSW S  = L;

        if (x1 < state.left) {
            x1 = state.left - x1;
            S.z += dS.z * x1;
            step(S, dS, x1);
            x1 = state.left;
        }
        if (x2 > state.right) x2 = state.right;
#if OL_RENDER_PROFILE
        ++ol_render_profile.spans;
        if (x2 > x1) ol_render_profile.pixels += x2 - x1;
#endif

        int32 i = y * state.width;

        if (state.depth_test) {
            if(state.depth_write)
                depth_span<true>(state.color+i+x1,state.depth+i+x1,x2-x1,S.z,dS.z,
                    S.u,S.v,S.l,dS.u,dS.v,dS.l,state.tile,state.lightmap,state.palette);
            else
                depth_span<false>(state.color+i+x1,state.depth+i+x1,x2-x1,S.z,dS.z,
                    S.u,S.v,S.l,dS.u,dS.v,dS.l,state.tile,state.lightmap,state.palette);
        } else
            color_span(state.color+i+x1,x2-x1,S.u,S.v,S.l,dS.u,dS.v,dS.l,
                state.tile,state.lightmap,state.palette);
    }

    void drawPart(const VertexSW &a, const VertexSW &b, const VertexSW &c, const VertexSW &d) {
        VertexSW L, R, dL, dR;
        int32 minY, maxY;

        int32 f = c.y - a.y;
        dL = (c - a) / f;
        dR = (d - b) / f;

        L = a;
        R = b;

        minY = a.y;
        maxY = c.y;

        if (maxY < state.top || minY >= state.bottom) return;

        if (minY < state.top) {
            minY = state.top - minY;
            L.x += dL.x * minY;
            L.z += dL.z * minY;
            R.x += dR.x * minY;
            R.z += dR.z * minY;
            step(L, dL, minY);
            step(R, dR, minY);
            minY = state.top;
        }

        if (maxY > state.bottom) maxY = state.bottom;

        for (int y = minY; y < maxY; y++) {
            drawLine(L, R, y);
            L.x += dL.x;
            L.z += dL.z;
            R.x += dR.x;
            R.z += dR.z;
            step(L, dL);
            step(R, dR);
        }
    }

    void drawTriangle(const Index *indices) {
    /*
             t
            /\ <----- top triangle
         m /__\/ n
           \  /\
             \  \ <-- bottom triangle
               \ \
                 \\
                   \
                    b
    */
        VertexSW _n;
        VertexSW *t = vertices + indices[0];
        VertexSW *m = vertices + indices[1];
        VertexSW *b = vertices + indices[2];
        VertexSW *n = &_n;

        if (checkBackface(t, m, b))
            return;

        int32 cx1 = state.left << 16;
        int32 cx2 = state.right << 16;

        if (t->x < cx1 && m->x < cx1 && b->x < cx1)
            return;

        if (t->x > cx2 && m->x > cx2 && b->x > cx2)
            return;

        sortVertices(t, m, b);

        if (b->y < state.top || t->y > state.bottom)
            return;

        *n = ((*b - *t) / (b->y - t->y) * (m->y - t->y)) + *t;
        n->y = m->y;

        if (m->x > n->x) {
            swap(m, n);
        }

        if (m->y != t->y) drawPart(*t, *t, *m, *n);
        if (m->y != b->y) drawPart(*m, *n, *b, *b);
    }

    void drawQuad(const Index *indices) {
    /*
             t
            /\ <----- top triangle
         m /__\/ n
           \  /\
            \   \ <-- quad
           p \/__\ o
             /\  /
               \/ <-- bottom triangle
               b
    */
        VertexSW _n;
        VertexSW _p;
        VertexSW *t = vertices + indices[0];
        VertexSW *m = vertices + indices[1];
        VertexSW *b = vertices + indices[2];
        VertexSW *o = vertices + indices[3];
        VertexSW *n = &_n;
        VertexSW *p = &_p;

        if (checkBackface(t, m, b))
            return;

        int32 cx1 = state.left << 16;
        int32 cx2 = state.right << 16;

        if (t->x < cx1 && m->x < cx1 && o->x < cx1 && b->x < cx1)
            return;

        if (t->x > cx2 && m->x > cx2 && o->x > cx2 && b->x > cx2)
            return;

        sortVertices(t, m, b, o);

        if (b->y < state.top || t->y > state.bottom)
            return;

        if (checkBackface(t, b, m) == checkBackface(t, b, o)) {

            VertexSW d = (*b - *t) / (b->y - t->y);

            *n = *t + d * (m->y - t->y);
            *p = *t + d * (o->y - t->y);

            n->y = m->y;
            p->y = o->y;

        } else {

            if (o->y != t->y) {
                *n = *t + ((*o - *t) / (o->y - t->y) * (m->y - t->y));
                n->y = m->y;
            }

            if (m->y != b->y) {
                *p = *b + ((*m - *b) / (m->y - b->y) * (o->y - b->y));
                p->y = o->y;
            }

        }

        if (o->y != t->y && m->x > n->x) swap(m, n);
        if (m->y != b->y && p->x > o->x) swap(p, o);

        if (t->y != m->y) drawPart(*t, *t, *m, *n);
        if (m->y != o->y) drawPart(*m, *n, *p, *o);
        if (o->y != b->y) drawPart(*p, *o, *b, *b);
    }

};
}

void ol_classic_draw(const OlRasterState &state, OlRasterVertex *vertices,
    const uint16_t *indices, const int32_t *quads, int quad_count,
    const int32_t *triangles, int triangle_count) {
    RasterBatch raster{state,vertices};
    for(int i=0;i<quad_count;++i)raster.drawQuad(indices+quads[i]);
    for(int i=0;i<triangle_count;++i)raster.drawTriangle(indices+triangles[i]);
}
