/* GPL-2.0-or-later */
#include "video_renderer.h"
#include <string.h>

void nes_video_reset(nes_video_renderer_t *r) {
    r->valid[0]=r->valid[1]=r->history_valid=0;
    memset(r->dirty,255,sizeof(r->dirty));
}
void nes_video_invalidate(nes_video_renderer_t *r,unsigned buffer) {
    if(buffer<2)r->valid[buffer]=0;
}
void nes_video_init(nes_video_renderer_t *r,unsigned w,unsigned h,
                    void *history,size_t bytes,const nes_controls_t *c) {
    nes_video_init_in_rect(r,w,h,(nes_display_rect_t){0,0,w,h},history,bytes,c);
}
void nes_video_init_in_rect(nes_video_renderer_t *r,unsigned w,unsigned h,
                    nes_display_rect_t area,void *history,size_t bytes,const nes_controls_t *c) {
    memset(r,0,sizeof(*r));r->width=w;r->height=h;
    if(bytes>=NES_VIDEO_HISTORY_BYTES)r->history=history;
    if(!area.width || !area.height || area.x>=w || area.y>=h ||
       area.width>w-area.x || area.height>h-area.y)area=(nes_display_rect_t){0,0,w,h};
    r->view_width=area.width;r->view_height=area.height;
    if(area.width*240>area.height*256)r->view_width=area.height*256/240;
    else r->view_height=area.width*240/256;
    if(r->view_width>256){r->view_width=256;r->view_height=240;}
    if(!r->view_width)r->view_width=1;if(!r->view_height)r->view_height=1;
    r->view_x=area.x+(area.width-r->view_width)/2;r->view_y=area.y+(area.height-r->view_height)/2;
    for(unsigned x=0;x<r->view_width;++x)r->sx[r->view_x+x]=x*256/r->view_width;
    for(unsigned y=0;y<r->view_height;++y)r->sy[r->view_y+y]=y*240/r->view_height;
    for(unsigned x=0;x<=32;++x)r->xedge[x]=r->view_x+(x*8*r->view_width+255)/256;
    for(unsigned y=0;y<=30;++y)r->yedge[y]=r->view_y+(y*8*r->view_height+239)/240;
    for(unsigned ty=0;ty<30;++ty)for(unsigned tx=0;tx<32;++tx) {
        uint32_t bit=UINT32_C(1)<<tx;
        for(unsigned y=r->yedge[ty];y<r->yedge[ty+1];++y)
            for(unsigned x=r->xedge[tx];x<r->xedge[tx+1];++x)
                if(c->mask[y*w+x]){
                    r->overlay[ty]|=bit;
                    r->button_overlay[(c->mask[y*w+x]-1)/3][ty]|=bit;
                }
    }
    nes_video_reset(r);
}
typedef uint32_t alias_word_t __attribute__((__may_alias__));
typedef uint64_t alias_pair_t __attribute__((__may_alias__));
static int same_row(const uint8_t *a,const uint8_t *b) {
    if(!(((uintptr_t)a|(uintptr_t)b)&7)) {
        const alias_pair_t *x=(const alias_pair_t*)a,*y=(const alias_pair_t*)b;
        for(unsigned n=0;n<32;n+=4)
            if(x[n]!=y[n] || x[n+1]!=y[n+1] ||
               x[n+2]!=y[n+2] || x[n+3]!=y[n+3])return 0;
        return 1;
    }
    if(((uintptr_t)a|(uintptr_t)b)&3)return !memcmp(a,b,256);
    const alias_word_t *x=(const alias_word_t*)a,*y=(const alias_word_t*)b;
    for(unsigned n=0;n<64;n+=4)
        if(x[n]!=y[n] || x[n+1]!=y[n+1] ||
           x[n+2]!=y[n+2] || x[n+3]!=y[n+3])return 0;
    return 1;
}
static int same_tile_row(const uint8_t *a,const uint8_t *b) {
    if(((uintptr_t)a|(uintptr_t)b)&3)return !memcmp(a,b,8);
    const alias_word_t *x=(const alias_word_t*)a,*y=(const alias_word_t*)b;
    return x[0]==y[0] && x[1]==y[1];
}
/* Keep the new scan experimental until measured on ESP32-S3. */
#ifdef NES_EXPERIMENTAL_TILE_SCAN
static void detect(nes_video_renderer_t *r,const uint8_t *video,unsigned pitch) {
    for(unsigned ty=0;ty<30;++ty) {
        uint32_t pending=~(r->dirty[0][ty]&r->dirty[1][ty]),bits=0;
        for(unsigned y=ty*8;y<ty*8+8;++y) {
            const uint8_t *src=video+y*pitch;
            uint8_t *prev=r->history+y*256;
            /* An already dirty tile needs no more comparisons until a buffer
             * is painted. Still refresh history: the next frame may revert to
             * an earlier colour after that buffer's dirty bit is cleared. */
            if(pending && r->history_valid) {
                if(same_row(src,prev))continue;
                if(pending==UINT32_MAX) {
                    for(unsigned tx=0;tx<32;++tx)
                        if(!same_tile_row(src+tx*8,prev+tx*8))bits|=UINT32_C(1)<<tx;
                } else {
                    uint32_t check=pending;
                    while(check) {
                        unsigned tx=__builtin_ctz(check);
                        uint32_t bit=UINT32_C(1)<<tx;
                        check&=check-1;
                        if(!same_tile_row(src+tx*8,prev+tx*8))bits|=bit;
                    }
                }
                pending&=~bits;
            }
            memcpy(prev,src,256);
        }
        r->dirty[0][ty]|=bits;r->dirty[1][ty]|=bits;
    }
    r->history_valid=1;
}
#else
static void detect(nes_video_renderer_t *r,const uint8_t *video,unsigned pitch) {
    /* Inspect individual tiles only on changed rows. The WASI libc memcmp
     * uses byte loads, so use aligned 32-bit loads for the real core's rows,
     * with an exact unaligned fallback for other callers. */
    for(unsigned y=0;y<240;++y) {
        const uint8_t *src=video+y*pitch;
        uint8_t *prev=r->history+y*256;
        if(r->history_valid && same_row(src,prev))continue;
        unsigned ty=y/8;
        /* Once both buffers need all 32 tiles, subsequent changed scanlines
         * in this band cannot add work. Avoid 32 comparisons on each of them. */
        if((r->dirty[0][ty]&r->dirty[1][ty])!=UINT32_MAX) {
            uint32_t bits=0;
            for(unsigned tx=0;tx<32;++tx) {
                if(!same_tile_row(src+tx*8,prev+tx*8))bits|=UINT32_C(1)<<tx;
            }
            r->dirty[0][ty]|=bits;r->dirty[1][ty]|=bits;
        }
        memcpy(prev,src,256);
    }
    r->history_valid=1;
}
#endif
static void row(nes_video_renderer_t *r,uint16_t *dst,const uint8_t *src,
                 const uint8_t *styles,unsigned x,unsigned end,
                 const nes_controls_t *c) {
    if(styles) {
        if(x&1){dst[x]=c->lut[styles[x]][src[r->sx[x]]&63];++x;}
        for(;x+1<end;x+=2) {
            uint16_t pair[2]={c->lut[styles[x]][src[r->sx[x]]&63],
                              c->lut[styles[x+1]][src[r->sx[x+1]]&63]};
            __builtin_memcpy(dst+x,pair,sizeof(pair));
        }
        if(x<end)dst[x]=c->lut[styles[x]][src[r->sx[x]]&63];
    } else {
        /* Pair stores halve Guest bounds checks and RGB565 writes. memcpy
         * preserves alignment and aliasing rules on native test hosts. */
        if(x&1){dst[x]=c->palette[src[r->sx[x]]&63];++x;}
        for(;x+1<end;x+=2) {
            uint16_t pair[2]={c->palette[src[r->sx[x]]&63],
                              c->palette[src[r->sx[x+1]]&63]};
            __builtin_memcpy(dst+x,pair,sizeof(pair));
        }
        if(x<end)dst[x]=c->palette[src[r->sx[x]]&63];
    }
}
void nes_video_render(nes_video_renderer_t *r,uint16_t *out,unsigned buffer,
                      const uint8_t *video,unsigned pitch,
                      nes_controls_t *c,uint8_t pressed) {
    unsigned visible=nes_controls_visible(c),alpha=visible?c->alpha:0;
    unsigned changed_buttons=0;
    if(!r->valid[buffer] || r->alpha[buffer]!=alpha)
        changed_buttons=(1u<<NES_CONTROLS_COUNT)-1;
    else if(visible)for(unsigned i=0;i<NES_CONTROLS_COUNT;++i)
        if((r->pressed[buffer]^pressed)&c->buttons[i].bit)changed_buttons|=1u<<i;
    r->painted_pixels=r->painted_tiles=0;
    if(visible)nes_controls_update(c,pressed);
    /* The panel-sized canvas uses unscaled Host DMA. Clear only on invalidation;
     * between frames, static black bars need no writes or pixel-wide scans. */
    if(changed_buttons) {
        if(!r->valid[buffer]){
            memset(out,0,r->width*r->height*sizeof(*out));
            r->painted_pixels+=r->width*r->height;
        }
        for(unsigned i=0;i<NES_CONTROLS_COUNT;++i) {
          if(!(changed_buttons&(1u<<i)))continue;
          const nes_control_bounds_t *b=&c->paint_bounds[i];
          for(unsigned y=b->top;y<b->bottom;++y)for(unsigned x=b->left;x<b->right;++x) {
            if(x>=r->view_x && x<r->view_x+r->view_width &&
               y>=r->view_y && y<r->view_y+r->view_height)continue;
            unsigned pos=y*r->width+x;
            unsigned style=c->mask[pos];
            /* Labels may extend outside button rectangles, and another
             * button may overwrite them. Bounds include the actual glyphs. */
            if(style<i*3+1 || style>i*3+3)continue;
            out[pos]=visible?c->black_lut[style]:0;
            ++r->painted_pixels;
          }
        }
    }
    if(!visible && r->resolve &&
       r->resolve(out+r->view_y*r->width+r->view_x,r->width,
                  r->view_width,r->view_height,video,pitch)) {
        /* The Host writes the acquired GuestMapped Surface in place. It may
         * split disjoint cache lines across cores; it joins before returning.
         * A later touch/fallback frame must not reuse pre-native history. */
        r->history_valid=0;memset(r->dirty,255,sizeof(r->dirty));
        r->painted_pixels+=r->view_width*r->view_height;
        r->valid[buffer]=1;r->alpha[buffer]=0;r->pressed[buffer]=pressed;
        return;
    }
    if(r->history)detect(r,video,pitch);
    /* Adjacent dirty tiles share one scanline span. This preserves sequential
     * output writes and avoids returning to each row for every eight pixels. */
    for(unsigned ty=0;ty<30;++ty) {
      uint32_t changed_overlay=0;
      for(unsigned i=0;i<NES_CONTROLS_COUNT;++i)
          if(changed_buttons&(1u<<i))changed_overlay|=r->button_overlay[i][ty];
      uint32_t bits=(!r->history || !r->valid[buffer])?UINT32_MAX:
                    r->dirty[buffer][ty] | changed_overlay;
      r->dirty[buffer][ty]=0;
      while(bits) {
        unsigned first=__builtin_ctz(bits),end=first;
        if(bits==UINT32_MAX)end=32;
        else while(end<32 && (bits&(UINT32_C(1)<<end)))++end;
        uint32_t run=end==32?(UINT32_MAX<<first):((UINT32_C(1)<<end)-(UINT32_C(1)<<first));
        bits&=~run;
        unsigned left=r->xedge[first],right=r->xedge[end];
        for(unsigned y=r->yedge[ty];y<r->yedge[ty+1];++y) {
            const uint8_t *styles=visible && c->row_used[y]?c->mask+y*r->width:NULL;
            row(r,out+y*r->width,video+r->sy[y]*pitch,styles,left,right,c);
        }
        r->painted_pixels+=(right-left)*(r->yedge[ty+1]-r->yedge[ty]);
        r->painted_tiles+=end-first;
      }
    }
    r->valid[buffer]=1;r->alpha[buffer]=alpha;r->pressed[buffer]=pressed;
}
