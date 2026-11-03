/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "system_font.h"
#include "pxa_font.h"
#include "port.h"
#include "rpc.h"
#include <string.h>

#define CACHE_SLOTS 128
#define CACHE_WAYS 4
#define CELL_WIDTH 32
#define CELL_HEIGHT 48
typedef struct { int slot,pixels,bold;nj_font font; } Face;
typedef struct {
    unsigned valid,slot,character,age;
    int width,height,advance;
    uint8_t coverage[CELL_WIDTH*CELL_HEIGHT];
} CachedGlyph;
static Face faces[6];
static const nj_font *pointers[6];
static CachedGlyph cache[CACHE_SLOTS];
static unsigned misses,hits,cache_clock;

static int request(uint32_t cp,unsigned pixels,pxa_font_glyph_t *glyph) {
    uint8_t p[8]={0};pxa_store_u32(p,cp);pxa_store_u16(p+4,pixels);
    const pxa_event_t *event=port_rpc(PXA_FONT_SERVICE,PXA_FONT_GLYPH,p,sizeof(p));
    return event && pxa_font_parse_glyph(event,event->token,glyph) && !glyph->status;
}
static CachedGlyph *get(Face *face,nj_u2 ch) {
    unsigned base=(((unsigned)ch*2654435761u+(unsigned)face->slot*97u)%(CACHE_SLOTS/CACHE_WAYS))*CACHE_WAYS;
    CachedGlyph *entry=&cache[base];
    for(unsigned i=0;i<CACHE_WAYS;++i) {
        CachedGlyph *candidate=&cache[base+i];
        if(candidate->valid && candidate->slot==(unsigned)face->slot && candidate->character==ch) {
            hits++;candidate->age=++cache_clock;return candidate;
        }
        if(!candidate->valid || (entry->valid && candidate->age<entry->age))entry=candidate;
    }
    pxa_font_glyph_t glyph;
    if(!request(ch,face->pixels,&glyph))return NULL;
    misses++;
    int width=glyph.advance+face->bold;
    if(width<glyph.offset_x+glyph.width+face->bold)width=glyph.offset_x+glyph.width+face->bold;
    if(width<1)width=1;
    /* Validate before replacing a cache entry, including its metrics. */
    if(width>CELL_WIDTH || face->font.height>CELL_HEIGHT)return NULL;
    entry->valid=0;entry->width=width;
    entry->height=face->font.height;entry->advance=glyph.advance+face->bold;
    memset(entry->coverage,0,sizeof(entry->coverage));
    int top=face->font.ascent-glyph.height-glyph.offset_y;
    for(int y=0;y<glyph.height;y++)for(int x=0;x<glyph.width;x++) {
        int dx=x+glyph.offset_x,dy=y+top;
        if(dx<0 || dx>=entry->width || dy<0 || dy>=entry->height)continue;
        unsigned char alpha=glyph.coverage[y*glyph.width+x];
        unsigned at=dy*entry->width+dx;
        if(entry->coverage[at]<alpha)entry->coverage[at]=alpha;
        if(face->bold && dx+1<entry->width && entry->coverage[at+1]<alpha)entry->coverage[at+1]=alpha;
    }
    entry->valid=1;entry->slot=face->slot;entry->character=ch;entry->age=++cache_clock;
    return entry;
}
static const nj_u1 *lookup(void *ud,nj_u2 ch,int *w,int *h) {
    CachedGlyph *entry=get(ud,ch);if(!entry)return NULL;
    *w=entry->width;*h=entry->height;return entry->coverage;
}
static int advance(void *ud,nj_u2 ch) {
    CachedGlyph *entry=get(ud,ch);return entry?entry->advance:faces[((Face*)ud)->slot].font.glyph_w;
}
int system_fonts_init(void) {
    const int sizes[]={NJ_FONT_SIZE_SMALL,NJ_FONT_SIZE_MEDIUM,NJ_FONT_SIZE_LARGE};
    const int pixels[]={10,12,16};
    for(unsigned i=0;i<6;i++) {
        pxa_font_glyph_t glyph;if(!request('M',pixels[i/2],&glyph) || glyph.line_height>CELL_HEIGHT)return 0;
        Face *f=&faces[i];f->slot=i;f->pixels=pixels[i/2];f->bold=i&1;
        f->font=(nj_font){.name="PXA system",.height=glyph.line_height,.ascent=glyph.line_height-glyph.baseline,
            .descent=glyph.baseline,.style=f->bold?NJ_FONT_STYLE_BOLD:0,.size=sizes[i/2],.proportional=1,
            .first_char=1,.last_char=0,.glyph_w=glyph.advance,.glyph_h=glyph.line_height,
            .glyph_lookup=lookup,.coverage=1,.glyph_ud=f,.glyph_advance=advance,
            .glyph_ascent=glyph.line_height-glyph.baseline};
        pointers[i]=&f->font;
    }
    port_log("J2ME system font ready: sizes=%d,%d,%d cache=%u bytes",faces[0].font.height,faces[2].font.height,faces[4].font.height,(unsigned)sizeof(cache));
    return 1;
}
const nj_font *const *system_fonts(void) {return pointers;}
static nj_u2 next_codepoint(const unsigned char **cursor) {
    const unsigned char *s=*cursor;unsigned cp=*s++,more=0;
    if(cp>=0xc0 && cp<0xe0){cp&=31;more=1;}
    else if(cp>=0xe0 && cp<0xf0){cp&=15;more=2;}
    else if(cp>=0xf0){cp&=7;more=3;}
    while(more && *s && (*s&0xc0)==0x80){cp=(cp<<6)|(*s++&63);more--;}
    *cursor=s;return more || cp>0xffff?0xfffd:(nj_u2)cp;
}
int system_text_pixels(void) {return faces[0].pixels;}
int system_text_height(void) {return faces[0].font.height;}
int system_text_width(const char *utf8) {
    const unsigned char *s=(const unsigned char*)utf8;int width=0;
    for(unsigned i=0;*s && i<256;++i)width+=advance(&faces[0],next_codepoint(&s));
    return width;
}
void system_text_tint(uint16_t *pixels,int width,int height,int x,int y,int clip_width,const char *utf8,uint16_t color) {
    nj_surface surface={pixels,width,height,width*2,NJ_PIXFMT_RGB565,2};
    nj_gfx graphics;nj_gfx_init(&graphics,&surface);nj_gfx_set_font(&graphics,pointers[0]);
    uint32_t rgba=UINT32_C(0xff000000)|((color>>11)*255/31)<<16|(((color>>5)&63)*255/63)<<8|(color&31)*255/31;
    nj_gfx_set_color(&graphics,rgba);nj_gfx_set_clip(&graphics,x,y,clip_width,height-y);
    nj_u2 text[256];unsigned count=0;
    const unsigned char *s=(const unsigned char*)utf8;
    while(*s && count<256) {
        text[count++]=next_codepoint(&s);
    }
    nj_gfx_draw_text(&graphics,text,count,x,y,NJ_ANCHOR_TOP|NJ_ANCHOR_LEFT);
}
void system_text(uint16_t *pixels,int width,int height,int x,int y,int clip_width,const char *utf8) {
    system_text_tint(pixels,width,height,x,y,clip_width,utf8,0xffff);
}
