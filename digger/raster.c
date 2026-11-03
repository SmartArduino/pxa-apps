/*
 * ---------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42, (c) Poul-Henning Kamp): Maxim
 * Sobolev <sobomax@altavista.net> wrote this file. As long as you retain
 * this  notice you can  do whatever you  want with this stuff. If we meet
 * some day, and you think this stuff is worth it, you can buy me a beer in
 * return.
 * 
 * Maxim Sobolev
 * --------------------------------------------------------------------------- 
 */


/* The software raster below is extracted from the pinned upstream sdl_vid.c.
 * Surface/window glue and PXA getters are independent adaptation code.
 */
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#if defined(DIGGER_RASTER_REFERENCE_SDL)
#include <SDL.h>
#else
#include "surface-lite.h"
#endif
#include "def.h"
#include "hardware.h"
#include "alpha.h"
#include "title_gz.h"
extern const uint8_t *vgatable[];
extern void digger_port_draw(void);
static SDL_Surface *screen16;
static const int16_t xratio = 2;
static const int16_t yratio = 2;
static const int16_t yoffset = 0;
static const int16_t hratio = 2;
static const int16_t wratio = 2 * 4;
#define SCREEN_WIDTH 640
#define SCREEN_HEIGHT 400
#define DISPLAY_FRAME_SEQ_STARTING UINT32_MAX
#define virt2scrx(x) (x*xratio)
#define virt2scry(y) (y*yratio+yoffset)
#define virt2scrw(w) (w*wratio)
#define virt2scrh(h) (h*hratio)

/* palette1, normal intensity */
static const SDL_Color vga16_pal1[] = \
{{0,0,0,0},{0,0,128,0},{0,128,0,0},{0,128,128,0},{128,0,0,0},{128,0,128,0} \
,{128,64,0,0},{128,128,128,0},{64,64,64,0},{0,0,255,0},{0,255,0,0} \
,{0,255,255,0},{255,0,0,0},{255,0,255,0},{255,255,0,0},{255,255,255,0}};
/* palette1, high intensity */
static const SDL_Color vga16_pal1i[] = \
{{0,0,0,0},{0,0,255,0},{0,255,0,0},{0,255,255,0},{255,0,0,0},{255,0,255,0} \
,{255,128,0,0},{196,196,196,0},{128,128,128,0},{128,128,255,0},{128,255,128,0} \
,{128,255,255,0},{255,128,128,0},{255,128,255,0},{255,255,128,0},{255,255,255,0}};
/* palette2, normal intensity */
static const SDL_Color vga16_pal2[] = \
{{0,0,0,0},{0,128,0,0},{128,0,0,0},{128,64,0,0},{0,0,128,0},{0,128,128,0} \
,{128,0,128,0},{128,128,128,0},{64,64,64,0},{0,255,0,0},{255,0,0,0} \
,{255,255,0,0},{0,0,255,0},{0,255,255,0},{255,0,255,0},{255,255,255,0}};
/* palette2, high intensity */
static const SDL_Color vga16_pal2i[] = \
{{0,0,0,0},{0,255,0,0},{255,0,0,0},{255,128,0,0},{0,0,255,0},{0,255,255,0} \
,{255,0,255,0},{196,196,196,0},{128,128,128,0},{128,255,128,0},{255,128,128,0} \
,{255,255,128,0},{128,128,255,0},{128,255,255,0},{255,128,255,0},{255,255,255,0}};

static const SDL_Color *npalettes[] = {vga16_pal1, vga16_pal2};
static const SDL_Color *ipalettes[] = {vga16_pal1i, vga16_pal2i};
static int16_t	currpal=0;

struct ch2bmap_plane {
	uint8_t const * const *sprites;
	SDL_Surface *caches[256];
};

static struct ch2bmap_plane sprites = {.sprites = vgatable};
static struct ch2bmap_plane alphas = {.sprites = ascii2vga};

static SDL_Surface *
ch2bmap(struct ch2bmap_plane *planep, uint8_t sprite, int16_t w, int16_t h)
{
	int16_t realw, realh;
	SDL_Surface *tmp;
        const uint8_t *sp;

	if (planep->caches[sprite] != NULL) {
		return (planep->caches[sprite]);
	}
	realw = virt2scrw(w);
	realh = virt2scrh(h);
        sp = planep->sprites[sprite];
	tmp = SDL_CreateRGBSurfaceFrom((void *)sp, realw, realh, 8, realw, 0, 0, 0, 0);
	SDL_SetPaletteColors(tmp->format->palette, npalettes[0], 0, 16);
	planep->caches[sprite] = tmp;
	return(tmp);
}

void vgaclear(void)
{
	SDL_Surface *tmp = NULL;
	
	vgageti(0, 0, (uint8_t *)&tmp, 80, 200);
	memset(tmp->pixels, 0x00, tmp->w*tmp->h);
	vgaputi(0, 0, (uint8_t *)&tmp, 80, 200);
	SDL_FreeSurface(tmp);
}

static void
setpal(const SDL_Color *pal)
{

	SDL_SetPaletteColors(screen16->format->palette, pal, 0, 16);
}

void vgainten(int16_t inten)
{
	if(inten == 1)
		setpal(ipalettes[currpal]);
	else
		setpal(npalettes[currpal]);
}

void vgapal(int16_t pal)
{
	setpal(npalettes[pal]);
	currpal = pal;
}

void vgaputi(int16_t x, int16_t y, uint8_t *p, int16_t w, int16_t h)
{
	SDL_Surface *tmp;
	SDL_Palette *reserv;
        SDL_Rect rect;

	rect.x = virt2scrx(x);
	rect.y = virt2scry(y);
	rect.w = virt2scrw(w);
	rect.h = virt2scrh(h);

	memcpy(&tmp, p, (sizeof (SDL_Surface *)));
	reserv = tmp->format->palette;
	tmp->format->palette = screen16->format->palette;
	SDL_BlitSurface(tmp, NULL, screen16, &rect);
	tmp->format->palette = reserv;
}

void vgageti(int16_t x, int16_t y, uint8_t *p, int16_t w, int16_t h)
{
	SDL_Surface *tmp;
	SDL_Rect src;
	
	memcpy(&tmp, p, (sizeof (SDL_Surface *)));
	if (tmp != NULL)
		SDL_FreeSurface(tmp); /* Destroy previously allocated bitmap */

	src.x = virt2scrx(x);
	src.y = virt2scry(y);
	src.w = virt2scrw(w);
	src.h = virt2scrh(h);

	tmp = SDL_CreateRGBSurface(0, src.w, src.h, 8, 0, 0, 0, 0);
	SDL_SetPaletteColors(tmp->format->palette, screen16->format->palette->colors, 0,
            screen16->format->palette->ncolors);
	SDL_BlitSurface(screen16, &src, tmp, NULL);
	memcpy(p, &tmp, (sizeof (SDL_Surface *)));
}

int16_t vgagetpix(int16_t x, int16_t y)
{
#if !defined(DIGGER_RASTER_REFERENCE_SDL)
  /* One query reads an 8x2 real-pixel footprint (four logical pixels). */
  if(x<0||y<0||x>316||y>199)return 0xff;
#endif

	int16_t sx, sy, sw, sh;
	int16_t xi, yi;
	int16_t rval = 0;
	uint8_t *base;

	if ((x > 319) || (y > 199))
	       return (0xff);

	/* Read directly from screen16 instead of allocating a temporary
	 * surface per call. virt2scr maps virtual 1x1 to 8x2 real pixels. */
	sx = virt2scrx(x);
	sy = virt2scry(y);
	sw = virt2scrw(1);
	sh = virt2scrh(1);

	base = (uint8_t *)screen16->pixels + sy * screen16->pitch + sx;
	for (yi = 0; yi < sh; yi++)
		for (xi = 0; xi < sw; xi++)
			if (base[yi * screen16->pitch + xi])
				rval |= 0x80 >> xi;

	return(rval & 0xee);
}

void vgaputim(int16_t x, int16_t y, int16_t ch, int16_t w, int16_t h)
{
	SDL_Surface *tmp;
	SDL_Surface *mask;
	SDL_Surface *scr = NULL;
	uint8_t   *tmp_pxl, *mask_pxl, *scr_pxl;
	int16_t realsize;
	int16_t i;

	tmp = ch2bmap(&sprites, ch*2, w, h);
	mask = ch2bmap(&sprites, ch*2+1, w, h);
	vgageti(x, y, (uint8_t *)&scr, w, h);
	realsize = scr->w * scr->h;
	tmp_pxl = (uint8_t *)tmp->pixels;
	mask_pxl = (uint8_t *)mask->pixels;
	scr_pxl = (uint8_t *)scr->pixels;
	for(i=0;i<realsize;i++)
		if(tmp_pxl[i] != 0xff)
			scr_pxl[i] = (scr_pxl[i] & mask_pxl[i]) | \
				tmp_pxl[i];

	vgaputi(x, y, (uint8_t *)&scr, w, h);
	SDL_FreeSurface(scr);
}

void vgawrite(int16_t x, int16_t y, int16_t ch, int16_t c)
{
	SDL_Surface *tmp;
	uint8_t copy_buf[24 * 24];
	uint8_t *orig, *copy;
	uint8_t color;
	int16_t w=3, h=12, size;
	int16_t i;

	if (!isvalchar(ch))
		return;
	tmp = ch2bmap(&alphas, ch-32, w, h);
	size = tmp->w*tmp->h;
	/* Max size: virt2scrw(3) * virt2scrh(12) = 24 * 24 = 576 bytes. */
	copy = copy_buf;
	memcpy(copy, tmp->pixels, size);

	for(i = size;i!=0;) {
		i--;
		color = copy[i];
		if (color==10) {
			if (c==2)
				color=12;
            		else {
              			if (c==3)
				color=14;
			}
          	} else
			if (color==12) {
				if (c==1)
					color=2;
				else
					if (c==2)
						color=4;
					else
						if (c==3)
							color=6;
			}
		copy[i] = color;
	}
	orig = (uint8_t*)tmp->pixels;
	tmp->pixels = copy;
	vgaputi(x, y, (uint8_t *)&tmp, w, h);
	tmp->pixels = orig;
}

void vgatitle(void)
{
	SDL_Surface *tmp=NULL;

	vgageti(0, 0, (uint8_t *)&tmp, 80, 200);
	gettitle((uint8_t*)tmp->pixels);
	vgaputi(0, 0, (uint8_t *)&tmp, 80, 200);
	SDL_FreeSurface(tmp);
}

void gretrace(void)
{
}

void savescreen(void)
{
/*	FILE *f;
	int i;
	
	f=fopen("screen.saw", "w");
	
	for(i=0;i<(VGLDisplay->Xsize*VGLDisplay->Ysize);i++)
		fputc(VGLDisplay->Bitmap[i], f);
	fclose(f);*/
}

void cgainit(void) {}

void cgaclear(void) {}

void cgatitle(void) {}

void cgawrite(int16_t x, int16_t y, int16_t ch, int16_t c)
  {(void)x; (void)y; (void)ch; (void)c;}

void cgaputim(int16_t x, int16_t y, int16_t ch, int16_t w, int16_t h)
  {(void)x; (void)y; (void)ch; (void)w; (void)h;}

void cgageti(int16_t x, int16_t y, uint8_t *p, int16_t w, int16_t h)
  {(void)x; (void)y; (void)p; (void)w; (void)h;}

void cgaputi(int16_t x, int16_t y, uint8_t *p, int16_t w, int16_t h)
  {(void)x; (void)y; (void)p; (void)w; (void)h;}

void cgapal(int16_t pal) {(void)pal;}

void cgainten(int16_t inten) {(void)inten;}

int16_t cgagetpix(int16_t x, int16_t y) {(void)x; (void)y; return(0);}
void vgainit(void)
{
  if(!screen16)screen16=SDL_CreateRGBSurface(0,SCREEN_WIDTH,SCREEN_HEIGHT,8,0,0,0,0);
  assert(screen16);
  currpal=0;
  SDL_SetPaletteColors(screen16->format->palette,npalettes[0],0,16);
}
void doscreenupdate(bool wait_for_present)
{
  (void)wait_for_present;
  digger_port_draw();
}
void switchmode(void) { digger_port_draw(); }
void sdl_enable_fullscreen(void) { }
void digger_raster_reset(void)
{
  for(unsigned i=0;i<256;i++){
    SDL_FreeSurface(sprites.caches[i]);sprites.caches[i]=NULL;
    SDL_FreeSurface(alphas.caches[i]);alphas.caches[i]=NULL;
  }
  SDL_FreeSurface(screen16);screen16=NULL;currpal=0;
}
void graphicsoff(void) { digger_raster_reset(); }
const uint8_t *digger_pixels640(void)
{
  return screen16?(const uint8_t *)screen16->pixels:NULL;
}
const uint16_t *digger_palette565(void)
{
  static uint16_t rgb565[16];
  for(unsigned i=0;i<16;i++){
    SDL_Color c=screen16?screen16->format->palette->colors[i]:npalettes[0][i];
    rgb565[i]=(uint16_t)(((c.r>>3)<<11)|((c.g>>2)<<5)|(c.b>>3));
  }
  return rgb565;
}
