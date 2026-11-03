/* SPDX-License-Identifier: MIT
 * Independent SDL1 software implementation for Jinyong, using public SDL1 ABI.
 * No SDLPoP/GPL adapter implementation is copied into this component.
 */
#include "jy_sdl.h"
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#define JY_SURFACE_MAGIC UINT32_C(0x4a595331)
#define JY_SURFACE_LIMIT (16u*1024u*1024u)
typedef struct {uint32_t magic;
 int owns_pixels;
 } surface_private;
static surface_private *check(SDL_Surface *s){
 if(!s||!s->map||((surface_private *)s->map)->magic!=JY_SURFACE_MAGIC){jy_sdl_fail("Invalid software surface");
  return NULL;
  }return s->map;
}
static unsigned shift(Uint32 mask){unsigned n=0;
 if(mask)while(!(mask&1)){mask>>=1;
  ++n;
  }
 return n;
 }
static unsigned bits(Uint32 mask){unsigned n=0;
 while(mask){n+=mask&1;
  mask>>=1;
  }return n;
 }
static int masks(int depth,Uint32 r,Uint32 g,Uint32 b,Uint32 a){
 if(depth==8)return !(r|g|b|a);
 if((r&g)||(r&b)||(r&a)||(g&b)||(g&a)||(b&a))return 0;
 Uint32 parts[]={r,g,b,a};
 for(unsigned n=0;n<4;++n){unsigned count=bits(parts[n]);
  if(count>8||(!count&&n<3))return 0;
  if(count&&parts[n]!=(((UINT32_C(1)<<count)-1)<<shift(parts[n])))return 0;
  }
 return depth==32||!((r|g|b|a)>>depth);
}
static SDL_Surface *allocate_surface(Uint32 flags,int w,int h,int depth,int pitch,void *pixels,Uint32 r,Uint32 g,Uint32 b,Uint32 a,int external){
 if(w<0||h<0||w>16384||h>16384||(depth!=8&&depth!=15&&depth!=16&&depth!=24&&depth!=32)){jy_sdl_fail("Unsupported software surface dimensions/depth");
  return NULL;
  }
 if(depth!=8&&!(r|g|b|a)){if(depth==15){r=0x7c00;
   g=0x3e0;
   b=0x1f;
   }else if(depth==16){r=0xf800;
   g=0x7e0;
   b=0x1f;
   }else{r=0xff0000;
   g=0xff00;
   b=0xff;
   }}
 if(!masks(depth,r,g,b,a)){jy_sdl_fail("Unsupported or overlapping pixel masks");
  return NULL;
  }
 unsigned bytes=(unsigned)(depth+7)/8;
 size_t minimum=(size_t)w*bytes;
 if(!external)pitch=(int)((minimum+3)&~(size_t)3);
 if(pitch<0||(size_t)pitch<minimum||pitch>65535||(size_t)pitch*(size_t)h>JY_SURFACE_LIMIT||(external&&!pixels&&w&&h)){jy_sdl_fail("Software surface pitch/allocation limit");
  return NULL;
  }
 SDL_Surface *s=calloc(1,sizeof *s);
 SDL_PixelFormat *f=calloc(1,sizeof *f);
 surface_private *m=calloc(1,sizeof *m);
 if(!s||!f||!m){free(s);
  free(f);
  free(m);
  jy_sdl_fail("Out of memory creating software surface");
  return NULL;
  }
 s->format=f;
 s->map=m;
 m->magic=JY_SURFACE_MAGIC;
 s->w=w;
 s->h=h;
 s->pitch=(Uint16)pitch;
 s->refcount=1;
 s->clip_rect=(SDL_Rect){0,0,(Uint16)w,(Uint16)h};
 f->BitsPerPixel=(Uint8)depth;
 f->BytesPerPixel=(Uint8)bytes;
 f->Rmask=r;
 f->Gmask=g;
 f->Bmask=b;
 f->Amask=a;
 f->Rshift=shift(r);
 f->Gshift=shift(g);
 f->Bshift=shift(b);
 f->Ashift=shift(a);
 f->Rloss=8-bits(r);
 f->Gloss=8-bits(g);
 f->Bloss=8-bits(b);
 f->Aloss=8-bits(a);
 f->alpha=255;
 s->flags=(flags&SDL_SRCALPHA)|(a?SDL_SRCALPHA:0)|(external?SDL_PREALLOC:0);
 s->pixels=pixels;
 m->owns_pixels=!external;
 if(!external){size_t allocation=(size_t)pitch*(size_t)h;
  s->pixels=calloc(allocation?allocation:1,1);
  }
 if(depth==8){f->palette=calloc(1,sizeof *f->palette);
  if(f->palette){f->palette->ncolors=256;
   f->palette->colors=calloc(256,sizeof(SDL_Color));
   if(f->palette->colors)for(unsigned i=0;i<256;++i)f->palette->colors[i]=(SDL_Color){255,255,255,0};
   }}
 if(!s->pixels||(depth==8&&(!f->palette||!f->palette->colors))){SDL_FreeSurface(s);
  jy_sdl_fail("Out of memory allocating software pixels/palette");
  return NULL;
  }
 return s;
}
SDL_Surface *SDL_CreateRGBSurface(Uint32 flags,int w,int h,int depth,Uint32 r,Uint32 g,Uint32 b,Uint32 a){return allocate_surface(flags,w,h,depth,0,NULL,r,g,b,a,0);
 }
SDL_Surface *SDL_CreateRGBSurfaceFrom(void *p,int w,int h,int depth,int pitch,Uint32 r,Uint32 g,Uint32 b,Uint32 a){return allocate_surface(0,w,h,depth,pitch,p,r,g,b,a,1);
 }
int jy_sdl_own_pixels(SDL_Surface *s){
 surface_private *m=check(s);if(!m||m->owns_pixels)return -1;
 m->owns_pixels=1;s->flags&=~SDL_PREALLOC;return 0;
}
void SDL_FreeSurface(SDL_Surface *s){
 if(!s)return;
 surface_private *m=check(s);
 if(!m)return;
 if(s->refcount>1){--s->refcount;
  return;
  }jy_sdl_surface_freed(s);
 if(m->owns_pixels)free(s->pixels);
 if(s->format){if(s->format->palette){free(s->format->palette->colors);
   free(s->format->palette);
   }free(s->format);
  }m->magic=0;
 free(m);
 free(s);
}
int SDL_LockSurface(SDL_Surface *s){if(!check(s))return -1;
 ++s->locked;
 return 0;
 }
void SDL_UnlockSurface(SDL_Surface *s){if(check(s)&&s->locked)--s->locked;
 }
Uint32 jy_sdl_pixel(const SDL_Surface *s,int x,int y){
 const Uint8 *p=(const Uint8 *)s->pixels+(size_t)y*s->pitch+(size_t)x*s->format->BytesPerPixel;
 Uint32 value=0;
 switch(s->format->BytesPerPixel){case 1:return *p;
  case 2:{Uint16 small;
   memcpy(&small,p,2);
   return small;
   }case 3:
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
 return ((Uint32)p[0]<<16)|((Uint32)p[1]<<8)|p[2];
#else
 return p[0]|((Uint32)p[1]<<8)|((Uint32)p[2]<<16);
#endif
 default:memcpy(&value,p,4);
  return value;
  }
}
void jy_sdl_put_pixel(SDL_Surface *s,int x,int y,Uint32 value){
 Uint8 *p=(Uint8 *)s->pixels+(size_t)y*s->pitch+(size_t)x*s->format->BytesPerPixel;
 switch(s->format->BytesPerPixel){case 1:*p=(Uint8)value;
  break;
  case 2:{Uint16 small=(Uint16)value;
   memcpy(p,&small,2);
   break;
   }case 3:
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
 p[0]=(Uint8)(value>>16);
  p[1]=(Uint8)(value>>8);
  p[2]=(Uint8)value;
#else
 p[0]=(Uint8)value;
  p[1]=(Uint8)(value>>8);
  p[2]=(Uint8)(value>>16);
#endif
 break;
  default:memcpy(p,&value,4);
  break;
  }
}
static Uint8 channel(Uint32 pixel,Uint32 mask){
 unsigned n=bits(mask);
 if(!n)return 0;
 Uint32 value=(pixel&mask)>>shift(mask);
 return (Uint8)(value*255/((UINT32_C(1)<<n)-1));
}
Uint32 SDL_MapRGBA(const SDL_PixelFormat *f,Uint8 r,Uint8 g,Uint8 b,Uint8 a){
 if(!f)return 0;
 if(f->palette){unsigned best=UINT_MAX,index=0;
  for(int i=0;i<f->palette->ncolors;++i){SDL_Color c=f->palette->colors[i];
   int dr=(int)c.r-r,dg=(int)c.g-g,db=(int)c.b-b;
   unsigned distance=(unsigned)(dr*dr+dg*dg+db*db);
   if(distance<best){best=distance;
    index=(unsigned)i;
    if(!distance)break;
    }}return index;
  }
 return (((Uint32)r>>f->Rloss)<<f->Rshift&f->Rmask)|(((Uint32)g>>f->Gloss)<<f->Gshift&f->Gmask)|(((Uint32)b>>f->Bloss)<<f->Bshift&f->Bmask)|(((Uint32)a>>f->Aloss)<<f->Ashift&f->Amask);
}
Uint32 SDL_MapRGB(const SDL_PixelFormat *f,Uint8 r,Uint8 g,Uint8 b){return SDL_MapRGBA(f,r,g,b,255);
 }
void SDL_GetRGBA(Uint32 pixel,const SDL_PixelFormat *f,Uint8 *r,Uint8 *g,Uint8 *b,Uint8 *a){
 Uint8 red=0,green=0,blue=0,alpha=255;
 if(f){if(f->palette){if(pixel<(Uint32)f->palette->ncolors){SDL_Color c=f->palette->colors[pixel];
    red=c.r;
    green=c.g;
    blue=c.b;
    }}else{red=channel(pixel,f->Rmask);
   green=channel(pixel,f->Gmask);
   blue=channel(pixel,f->Bmask);
   if(f->Amask)alpha=channel(pixel,f->Amask);
   }}
 if(r)*r=red;
 if(g)*g=green;
 if(b)*b=blue;
 if(a)*a=alpha;
}
void SDL_GetRGB(Uint32 pixel,const SDL_PixelFormat *f,Uint8 *r,Uint8 *g,Uint8 *b){SDL_GetRGBA(pixel,f,r,g,b,NULL);
 }
int SDL_SetPalette(SDL_Surface *s,int flags,SDL_Color *colors,int first,int count){
 (void)flags;
 if(!check(s)||!s->format->palette||!colors||first<0||count<0||first>=s->format->palette->ncolors)return 0;
 int n=count;
 if(n>s->format->palette->ncolors-first)n=s->format->palette->ncolors-first;
 memcpy(s->format->palette->colors+first,colors,(size_t)n*sizeof *colors);
 ++s->format_version;
 return n==count;
}
int SDL_SetColors(SDL_Surface *s,SDL_Color *c,int first,int count){return SDL_SetPalette(s,SDL_LOGPAL|SDL_PHYSPAL,c,first,count);
 }
int SDL_SetColorKey(SDL_Surface *s,Uint32 flags,Uint32 key){if(!check(s))return -1;
 s->format->colorkey=key;
 if(flags&SDL_SRCCOLORKEY)s->flags|=SDL_SRCCOLORKEY;
 else s->flags&=~SDL_SRCCOLORKEY;
 return 0;
 }
int SDL_SetAlpha(SDL_Surface *s,Uint32 flags,Uint8 alpha){if(!check(s))return -1;
 s->format->alpha=alpha;
 if(flags&SDL_SRCALPHA)s->flags|=SDL_SRCALPHA;
 else s->flags&=~SDL_SRCALPHA;
 return 0;
 }
static int intersect(int *x,int *y,int *w,int *h,int left,int top,int right,int bottom){
 if(*x<left){*w-=left-*x;
  *x=left;
  }if(*y<top){*h-=top-*y;
  *y=top;
  }if(*w>right-*x)*w=right-*x;
 if(*h>bottom-*y)*h=bottom-*y;
 if(*w<0)*w=0;
 if(*h<0)*h=0;
 return *w>0&&*h>0;
}
SDL_bool SDL_SetClipRect(SDL_Surface *s,const SDL_Rect *rect){
 if(!check(s))return SDL_FALSE;
 int x=rect?rect->x:0,y=rect?rect->y:0,w=rect?rect->w:s->w,h=rect?rect->h:s->h;
 int nonempty=intersect(&x,&y,&w,&h,0,0,s->w,s->h);
 s->clip_rect=(SDL_Rect){(Sint16)x,(Sint16)y,(Uint16)w,(Uint16)h};
 return nonempty?SDL_TRUE:SDL_FALSE;
}
void SDL_GetClipRect(SDL_Surface *s,SDL_Rect *rect){if(check(s)&&rect)*rect=s->clip_rect;
 }
int SDL_FillRect(SDL_Surface *s,SDL_Rect *rect,Uint32 pixel){
 if(!check(s))return -1;
 int x=rect?rect->x:0,y=rect?rect->y:0,w=rect?rect->w:s->w,h=rect?rect->h:s->h;
 if(!intersect(&x,&y,&w,&h,s->clip_rect.x,s->clip_rect.y,s->clip_rect.x+s->clip_rect.w,s->clip_rect.y+s->clip_rect.h))return 0;
 for(int row=y;row<y+h;++row)for(int col=x;col<x+w;++col)jy_sdl_put_pixel(s,col,row,pixel);
 return 0;
}
static Uint8 blend_rgb(Uint8 source,Uint8 dest,Uint8 alpha){if(alpha==255)return source;
 if(!alpha)return dest;
 return (Uint8)((int)dest+(((int)source-dest)*(int)alpha>>8));
 }
static Uint8 blend_pixel(Uint8 source,Uint8 dest,Uint8 alpha){if(alpha==255)return source;
 if(!alpha)return dest;
 return (Uint8)(((unsigned)source*alpha>>8)+((unsigned)dest*(255-alpha)>>8));
 }
static Uint8 blend_indexed(Uint8 source,Uint8 dest,Uint8 alpha){return (Uint8)((int)dest+((int)source-dest)*(int)alpha/255);
 }
static Uint32 quantized_palette_pixel(const SDL_PixelFormat *format,Uint8 r,Uint8 g,Uint8 b){return SDL_MapRGB(format,(Uint8)((r>>5)*255/7),(Uint8)((g>>5)*255/7),(Uint8)((b>>6)*255/3));
 }
static int is_565(const SDL_PixelFormat *format){return format->BitsPerPixel==16&&format->Rmask==0xf800&&format->Gmask==0x7e0&&format->Bmask==0x1f&&!format->Amask;
 }
static Uint8 fast_565_green(Uint32 pixel){unsigned value=(pixel&0x7e0)>>5;
 return (Uint8)(4*value+value/24);
 }
static Uint32 blend_quantized(const SDL_PixelFormat *format,Uint32 source,Uint32 dest,unsigned alpha5){
 Uint32 result=0,channels[]={format->Rmask,format->Gmask,format->Bmask};
 for(unsigned n=0;n<3;++n){unsigned offset=shift(channels[n]);
  int s=(int)((source&channels[n])>>offset),d=(int)((dest&channels[n])>>offset);
 int out=alpha5==31?s:d+((s-d)*(int)alpha5>>5);
  result|=((Uint32)out<<offset)&channels[n];
  }return result;
}
int SDL_UpperBlit(SDL_Surface *src,SDL_Rect *source_rect,SDL_Surface *dst,SDL_Rect *dest_rect){
 if(!check(src)||!check(dst))return -1;
 int sx=source_rect?source_rect->x:0,sy=source_rect?source_rect->y:0,w=source_rect?source_rect->w:src->w,h=source_rect?source_rect->h:src->h,dx=dest_rect?dest_rect->x:0,dy=dest_rect?dest_rect->y:0;
 if(sx<0){dx-=sx;
  w+=sx;
  sx=0;
  }if(sy<0){dy-=sy;
  h+=sy;
  sy=0;
  }if(w>src->w-sx)w=src->w-sx;
 if(h>src->h-sy)h=src->h-sy;
 int before_x=dx,before_y=dy;
 int nonempty=intersect(&dx,&dy,&w,&h,dst->clip_rect.x,dst->clip_rect.y,dst->clip_rect.x+dst->clip_rect.w,dst->clip_rect.y+dst->clip_rect.h);
 sx+=dx-before_x;
 sy+=dy-before_y;
 if(dest_rect)*dest_rect=(SDL_Rect){(Sint16)dx,(Sint16)dy,(Uint16)w,(Uint16)h};
 if(!nonempty)return 0;
 if(src->format->palette&&(src->flags&SDL_SRCALPHA)&&src->format->alpha==255&&!dst->format->palette)return jy_sdl_fail("SDL1 indexed opaque-alpha combination is unsupported");
 Uint32 *snapshot=NULL;
 if(src==dst){size_t count=(size_t)w*(size_t)h;
  if(count>JY_SURFACE_LIMIT/4)return jy_sdl_fail("Overlap copy allocation limit");
  snapshot=malloc(count*sizeof *snapshot);
  if(!snapshot)return jy_sdl_fail("Out of memory for overlap copy");
  for(int y=0;y<h;++y)for(int x=0;x<w;++x)snapshot[(size_t)y*w+x]=jy_sdl_pixel(src,sx+x,sy+y);
  }
 for(int y=0;y<h;++y)for(int x=0;x<w;++x){
  Uint32 pixel=snapshot?snapshot[(size_t)y*w+x]:jy_sdl_pixel(src,sx+x,sy+y);
  Uint32 key_mask=src->format->palette?255:src->format->Rmask|src->format->Gmask|src->format->Bmask;
  /* The staged SDL1 reference disables color keys on alpha-channel surfaces,
   * even after SetAlpha(0). Preserve its actual SetColorKey/blit behavior. */
  if(!src->format->Amask&&(src->flags&SDL_SRCCOLORKEY)&&(pixel&key_mask)==(src->format->colorkey&key_mask))continue;
  Uint8 r,g,b,a;
  SDL_GetRGBA(pixel,src->format,&r,&g,&b,&a);
  if(is_565(src->format)&&dst->format->BytesPerPixel==4&&!(src->flags&(SDL_SRCCOLORKEY|SDL_SRCALPHA)))g=fast_565_green(pixel);
  if(src->flags&SDL_SRCALPHA){Uint8 alpha=src->format->Amask?a:src->format->alpha;
   if(!alpha)continue;
   if((dst->format->BitsPerPixel==15||dst->format->BitsPerPixel==16)&&!dst->format->Amask){
    if(src->format->Amask){unsigned alpha5=alpha>>3;
     if(!alpha5)continue;
     Uint32 mapped=SDL_MapRGB(dst->format,r,g,b);
     jy_sdl_put_pixel(dst,dx+x,dy+y,blend_quantized(dst->format,mapped,jy_sdl_pixel(dst,dx+x,dy+y),alpha5));
     continue;
     }
   }
   Uint8 dr,dg,db,da;
   SDL_GetRGBA(jy_sdl_pixel(dst,dx+x,dy+y),dst->format,&dr,&dg,&db,&da);
   if(src->format->Amask){r=blend_pixel(r,dr,alpha);
    g=blend_pixel(g,dg,alpha);
    b=blend_pixel(b,db,alpha);
    a=da;
    }
   else if(src->format->palette){r=blend_indexed(r,dr,alpha);
    g=blend_indexed(g,dg,alpha);
    b=blend_indexed(b,db,alpha);
    a=255;
    }
   else if(dst->format->BytesPerPixel==2){r=blend_indexed(r,dr,alpha);
    g=blend_indexed(g,dg,alpha);
    b=blend_indexed(b,db,alpha);
    a=255;
    }
   else{r=blend_rgb(r,dr,alpha);
    g=blend_rgb(g,dg,alpha);
    b=blend_rgb(b,db,alpha);
    a=255;
    }
  }
  jy_sdl_put_pixel(dst,dx+x,dy+y,dst->format->palette&&!src->format->palette?quantized_palette_pixel(dst->format,r,g,b):SDL_MapRGBA(dst->format,r,g,b,a));
 }
 free(snapshot);
 return 0;
}
int SDL_LowerBlit(SDL_Surface *s,SDL_Rect *sr,SDL_Surface *d,SDL_Rect *dr){return SDL_UpperBlit(s,sr,d,dr);
 }
SDL_Surface *SDL_ConvertSurface(SDL_Surface *source,SDL_PixelFormat *format,Uint32 flags){
 if(!check(source)||!format)return NULL;
 SDL_Surface *out=SDL_CreateRGBSurface(flags,source->w,source->h,format->BitsPerPixel,format->Rmask,format->Gmask,format->Bmask,format->Amask);
 if(!out)return NULL;
 if(format->palette&&out->format->palette)SDL_SetColors(out,format->palette->colors,0,format->palette->ncolors);
 for(int y=0;y<source->h;++y)for(int x=0;x<source->w;++x){Uint8 r,g,b,a;
  Uint32 original=jy_sdl_pixel(source,x,y);
  SDL_GetRGBA(original,source->format,&r,&g,&b,&a);
  if(is_565(source->format)&&out->format->BytesPerPixel==4)g=fast_565_green(original);
  Uint32 key_mask=source->format->palette?255:source->format->Rmask|source->format->Gmask|source->format->Bmask;
  if(out->format->Amask&&!source->format->Amask&&(source->flags&SDL_SRCCOLORKEY)&&(original&key_mask)==(source->format->colorkey&key_mask))a=0;
  jy_sdl_put_pixel(out,x,y,out->format->palette&&!source->format->palette?quantized_palette_pixel(out->format,r,g,b):SDL_MapRGBA(out->format,r,g,b,a));
  }
 if((flags&SDL_SRCCOLORKEY)&&(source->flags&SDL_SRCCOLORKEY)){Uint8 r,g,b;
  SDL_GetRGB(source->format->colorkey,source->format,&r,&g,&b);
  SDL_SetColorKey(out,SDL_SRCCOLORKEY,SDL_MapRGB(out->format,r,g,b));
  }
 if(flags&SDL_SRCALPHA)SDL_SetAlpha(out,SDL_SRCALPHA,source->format->alpha);
 return out;
}
SDL_Surface *SDL_DisplayFormat(SDL_Surface *source){SDL_Surface *video=SDL_GetVideoSurface();
 if(!video){jy_sdl_fail("DisplayFormat requires a video mode");
  return NULL;
  }return SDL_ConvertSurface(source,video->format,source->flags&(SDL_SRCCOLORKEY|SDL_SRCALPHA));
 }
SDL_Surface *SDL_DisplayFormatAlpha(SDL_Surface *source){
 if(!check(source))return NULL;
 if(!SDL_GetVideoSurface()){jy_sdl_fail("DisplayFormatAlpha requires a video mode");
  return NULL;
  }
 SDL_PixelFormat alpha_format={.BitsPerPixel=32,.BytesPerPixel=4,.Rmask=0xff0000,.Gmask=0xff00,.Bmask=0xff,.Amask=0xff000000};
 return SDL_ConvertSurface(source,&alpha_format,source->flags&SDL_SRCALPHA);
}
int jy_sdl_copy_rgb24(const SDL_Surface *surface,unsigned char *out,size_t pitch){
 if(!surface||!out||pitch<(size_t)surface->w*3)return jy_sdl_fail("Invalid RGB24 output buffer");
 for(int y=0;y<surface->h;++y)for(int x=0;x<surface->w;++x)SDL_GetRGB(jy_sdl_pixel(surface,x,y),surface->format,out+(size_t)y*pitch+x*3,out+(size_t)y*pitch+x*3+1,out+(size_t)y*pitch+x*3+2);
 return 0;
}
