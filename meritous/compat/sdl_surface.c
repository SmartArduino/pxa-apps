/* SPDX-License-Identifier: GPL-3.0-or-later
 * SDL 1.2's INDEX8 surface subset, implemented locally for the PXA Guest.
 */
#include <stdlib.h>
#include <string.h>
#include "SDL.h"
#include "SDL_image.h"
#include "pxa_log.h"
#include "../assets.h"

void mt_present(SDL_Surface *screen);
void mt_palette(SDL_Color *colors,int first,int count);

SDL_Surface *SDL_CreateRGBSurface(Uint32 flags,int w,int h,int depth,
        Uint32 r,Uint32 g,Uint32 b,Uint32 a) {
    (void)r;(void)g;(void)b;(void)a;
    if(w<=0 || h<=0 || depth!=8) return NULL;
    SDL_Surface *s=calloc(1,sizeof(*s));
    if(!s) return NULL;
    s->format=calloc(1,sizeof(*s->format));
    s->pixels=calloc((size_t)w,h);
    if(!s->format || !s->pixels) {SDL_FreeSurface(s); return NULL;}
    s->format->palette=calloc(1,sizeof(SDL_Palette));
    if(!s->format->palette) {SDL_FreeSurface(s);return NULL;}
    s->format->palette->colors=calloc(256,sizeof(SDL_Color));
    if(!s->format->palette->colors) {SDL_FreeSurface(s);return NULL;}
    s->format->palette->ncolors=256;
    for(int i=0;i<256;i++) s->format->palette->colors[i]=(SDL_Color){i,i,i,0};
    s->w=w;s->h=h;s->pitch=w;s->flags=flags;
    s->format->BitsPerPixel=s->format->BytesPerPixel=8;
    s->format->BytesPerPixel=1;s->format->alpha=255;
    s->clip_rect=(SDL_Rect){0,0,w,h};
    return s;
}
SDL_Surface *SDL_SetVideoMode(int w,int h,int bpp,Uint32 flags) {
    return SDL_CreateRGBSurface(flags,w,h,bpp,0,0,0,0);
}
void SDL_FreeSurface(SDL_Surface *s) {
    if(!s)return;
    if(s->format){if(s->format->palette){free(s->format->palette->colors);free(s->format->palette);}free(s->format);}
    free(s->pixels);free(s);
}
int SDL_BlitSurface(SDL_Surface *src,SDL_Rect *sr,SDL_Surface *dst,SDL_Rect *dr) {
    if(!src || !dst)return -1;
    int sx=sr?sr->x:0,sy=sr?sr->y:0,w=sr?sr->w:src->w,h=sr?sr->h:src->h;
    int dx=dr?dr->x:0,dy=dr?dr->y:0;
    if(sx<0){dx-=sx;w+=sx;sx=0;}if(sy<0){dy-=sy;h+=sy;sy=0;}
    if(dx<0){sx-=dx;w+=dx;dx=0;}if(dy<0){sy-=dy;h+=dy;dy=0;}
    if(w>src->w-sx)w=src->w-sx;if(h>src->h-sy)h=src->h-sy;
    if(w>dst->w-dx)w=dst->w-dx;if(h>dst->h-dy)h=dst->h-dy;
    if(w<=0 || h<=0)return 0;
    for(int y=0;y<h;y++){
        Uint8 *from=(Uint8*)src->pixels+(sy+y)*src->pitch+sx;
        Uint8 *to=(Uint8*)dst->pixels+(dy+y)*dst->pitch+dx;
        if(src->flags&SDL_SRCCOLORKEY){for(int x=0;x<w;x++)if(from[x]!=src->format->colorkey)to[x]=from[x];}
        else memmove(to,from,w);
    }
    if(dr){dr->w=w;dr->h=h;}return 0;
}
int SDL_FillRect(SDL_Surface *s,SDL_Rect *r,Uint32 color) {
    if(!s)return -1;
    int x=r?r->x:0,y=r?r->y:0,w=r?r->w:s->w,h=r?r->h:s->h;
    if(x<0){w+=x;x=0;}if(y<0){h+=y;y=0;}
    if(w>s->w-x)w=s->w-x;if(h>s->h-y)h=s->h-y;
    if(w<=0||h<=0)return 0;
    for(int j=0;j<h;j++)memset((Uint8*)s->pixels+(y+j)*s->pitch+x,color,w);
    return 0;
}
int SDL_SetColorKey(SDL_Surface *s,Uint32 flags,Uint32 key) {
    s->flags=(s->flags&~SDL_SRCCOLORKEY)|(flags&SDL_SRCCOLORKEY);s->format->colorkey=key;return 0;
}
int SDL_SetPalette(SDL_Surface *s,int flags,SDL_Color *colors,int first,int count) {
    if(first<0||count<0||first+count>256)return 0;
    if(flags&SDL_LOGPAL)memcpy(s->format->palette->colors+first,colors,count*sizeof(*colors));
    if(flags&SDL_PHYSPAL)mt_palette(colors,first,count);
    return 1;
}
void SDL_UpdateRect(SDL_Surface *s,Sint32 x,Sint32 y,Uint32 w,Uint32 h) {
    (void)x;(void)y;(void)w;(void)h;mt_present(s);
}
void SDL_WM_SetCaption(const char *title,const char *icon){(void)title;(void)icon;}
void SDL_WM_SetIcon(SDL_Surface *icon,Uint8 *mask){(void)icon;(void)mask;}
int SDL_SaveBMP(SDL_Surface *s,const char *name){(void)s;(void)name;return -1;}
void SDL_Quit(void){}
SDL_Surface *IMG_Load(const char *name) {
    if(!strcmp(name,"dat/i/title.png")) (void)pxa_log_write(PXA_LOG_LEVEL_INFO,"Meritous original MENU assets loaded");
    for(unsigned i=0;i<sizeof(mt_assets)/sizeof(mt_assets[0]);i++){
        const struct mt_asset *a=&mt_assets[i];
        if(!strcmp(name,a->name)&&a->w){
            SDL_Surface *s=SDL_CreateRGBSurface(0,a->w,a->h,8,0,0,0,0);
            if(s)memcpy(s->pixels,a->pixels,a->bytes);
            return s;
        }
    }
    return NULL;
}
const char *IMG_GetError(void){return "Original embedded image not found";}
