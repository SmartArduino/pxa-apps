/* GPL-3.0-or-later. See vendor/stb/stb_truetype.h for its separate licence. */
#include "SDL_ttf.h"
#include <math.h>
#define STB_TRUETYPE_IMPLEMENTATION
#include "vendor/stb/stb_truetype.h"
struct PxaSdlFont {
  stbtt_fontinfo info;
  unsigned char *owned;
  float scale;
  int ascent,descent,gap,height,bold;
};
static int initialized;
int TTF_Init(void) { ++initialized;return 0; }
int TTF_WasInit(void) { return initialized; }
void TTF_Quit(void) { if(initialized)--initialized; }
TTF_Font *TTF_OpenFontRW(SDL_RWops *rw,int close,int points) {
  if(!rw || points<1 || points>256) {if(rw&&close)SDL_RWclose(rw);return NULL;}
  TTF_Font *font=SDL_calloc(1,sizeof(*font));
  const unsigned char *data=NULL;
  Sint64 bytes=SDL_RWsize(rw);
  if(!font || bytes<12 || bytes>32*1024*1024)goto failed;
  if((rw->type==SDL_RWOPS_MEMORY || rw->type==SDL_RWOPS_MEMORY_RO) && !close)
    data=rw->hidden.mem.base;
  else {
    font->owned=SDL_malloc((size_t)bytes);
    if(!font->owned || SDL_RWseek(rw,0,RW_SEEK_SET)<0 ||
        SDL_RWread(rw,font->owned,1,(size_t)bytes)!=(size_t)bytes)goto failed;
    data=font->owned;
  }
  int offset=stbtt_GetFontOffsetForIndex(data,0);
  if(offset<0 || offset>=bytes || !stbtt_InitFont(&font->info,data,offset))goto failed;
  /* SDL_ttf points at 72 dpi scale the em square, not ascent+descent. */
  font->scale=stbtt_ScaleForMappingEmToPixels(&font->info,points);
  stbtt_GetFontVMetrics(&font->info,&font->ascent,&font->descent,&font->gap);
  font->height=(int)ceilf(font->ascent*font->scale)-(int)floorf(font->descent*font->scale);
  if(close)SDL_RWclose(rw);
  return font;
failed:
  if(close)SDL_RWclose(rw);
  if(font){SDL_free(font->owned);SDL_free(font);}
  SDL_SetError("Cannot load TrueType font");return NULL;
}
TTF_Font *TTF_OpenFont(const char *path,int points) {
  return TTF_OpenFontRW(SDL_RWFromFile(path,"rb"),1,points);
}
void TTF_CloseFont(TTF_Font *font) { if(font){SDL_free(font->owned);SDL_free(font);} }
void TTF_SetFontStyle(TTF_Font *font,int style) { if(font)font->bold=!!(style&TTF_STYLE_BOLD); }
int TTF_FontHeight(const TTF_Font *font) { return font?font->height:0; }
static unsigned next_codepoint(const unsigned char **cursor) {
  const unsigned char *p=*cursor;
  unsigned c=*p++,n=0;
  if(c<128){*cursor=p;return c;}
  if(c>=0xc2 && c<=0xdf){c&=31;n=1;}
  else if(c>=0xe0 && c<=0xef){c&=15;n=2;}
  else if(c>=0xf0 && c<=0xf4){c&=7;n=3;}
  else {*cursor=p;return 0xfffd;}
  for(unsigned i=0;i<n;++i) {
    if((*p&0xc0)!=0x80){*cursor=p;return 0xfffd;}
    c=(c<<6)|(*p++&63);
  }
  *cursor=p;
  return c>0x10ffff || (c>=0xd800 && c<=0xdfff) ||
    (n==1 && c<128) || (n==2 && c<2048) || (n==3 && c<65536)?0xfffd:c;
}
static int measure(TTF_Font *font,const char *text,int *width,int *height,int *left) {
  if(!font || !text)return SDL_SetError("Missing font/text");
  float x=0;int previous=0,min_x=0,max_x=0;
  for(const unsigned char *p=(const unsigned char*)text;*p;) {
    unsigned c=next_codepoint(&p);int advance,bearing;
    if(previous)x+=stbtt_GetCodepointKernAdvance(&font->info,previous,c)*font->scale;
    int x0,y0,x1,y1;
    stbtt_GetCodepointBitmapBox(&font->info,c,font->scale,font->scale,&x0,&y0,&x1,&y1);
    x0+=(int)floorf(x);x1+=(int)floorf(x);
    if(x0<min_x)min_x=x0;if(x1>max_x)max_x=x1;
    stbtt_GetCodepointHMetrics(&font->info,c,&advance,&bearing);
    x+=advance*font->scale;previous=c;
  }
  if((int)ceilf(x)>max_x)max_x=(int)ceilf(x);
  if(width)*width=max_x-min_x+font->bold;
  if(height)*height=font->height;
  if(left)*left=min_x;
  return 0;
}
int TTF_SizeUTF8(TTF_Font *font,const char *text,int *width,int *height) {
  return measure(font,text,width,height,NULL);
}
SDL_Surface *TTF_RenderUTF8_Blended(TTF_Font *font,const char *text,SDL_Color color) {
  int width,height,left;
  if(measure(font,text,&width,&height,&left))return NULL;
  if(width<1)width=1;
  SDL_Surface *surface=SDL_CreateRGBSurfaceWithFormat(0,width,height,32,SDL_PIXELFORMAT_RGBA32);
  if(!surface)return NULL;
  SDL_memset(surface->pixels,0,(size_t)surface->pitch*height);
  const int baseline=(int)ceilf(font->ascent*font->scale);
  float x=-left;int previous=0;
  for(const unsigned char *p=(const unsigned char*)text;*p;) {
    unsigned c=next_codepoint(&p);int advance,bearing,w,h,x0,y0;
    if(previous)x+=stbtt_GetCodepointKernAdvance(&font->info,previous,c)*font->scale;
    unsigned char *glyph=stbtt_GetCodepointBitmap(&font->info,font->scale,font->scale,c,&w,&h,&x0,&y0);
    if(glyph)for(int gy=0;gy<h;++gy)for(int gx=0;gx<w;++gx) {
      int dx=(int)floorf(x)+x0+gx,dy=baseline+y0+gy;
      if(dy<0 || dy>=height)continue;
      for(int bold=0;bold<=font->bold;++bold)if(dx+bold>=0 && dx+bold<width) {
        Uint8 *pixel=(Uint8*)surface->pixels+dy*surface->pitch+(dx+bold)*4;
        Uint8 alpha=glyph[gy*w+gx];
        pixel[0]=color.r;pixel[1]=color.g;pixel[2]=color.b;
        pixel[3]=alpha>pixel[3]?alpha:pixel[3];
      }
    }
    stbtt_FreeBitmap(glyph,NULL);
    stbtt_GetCodepointHMetrics(&font->info,c,&advance,&bearing);
    x+=advance*font->scale;previous=c;
  }
  SDL_SetSurfaceBlendMode(surface,SDL_BLENDMODE_BLEND);
  return surface;
}
