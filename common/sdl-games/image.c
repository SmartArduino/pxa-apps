/* GPL-3.0-or-later. The separately licensed stb implementation is unmodified. */
#include "SDL_image.h"
#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_STDIO
#define STBI_NO_HDR
#define STBI_NO_LINEAR
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STBI_ONLY_BMP
#include "vendor/stb/stb_image.h"
static int read_image(void *p, char *bytes, int n) { return SDL_RWread(p,bytes,1,n); }
static void skip_image(void *p, int n) { SDL_RWseek(p,n,RW_SEEK_CUR); }
static int end_image(void *p) { Sint64 n = SDL_RWsize(p); return n >= 0 && SDL_RWtell(p) >= n; }
int IMG_Init(int flags) { return flags & (IMG_INIT_PNG | IMG_INIT_JPG); }
void IMG_Quit(void) {}
SDL_Surface *IMG_Load_RW(SDL_RWops *rw, int close) {
  if (!rw) return NULL;
  stbi_io_callbacks io = {read_image,skip_image,end_image};
  int w=0,h=0,components=0;
  unsigned char *pixels = stbi_load_from_callbacks(&io,rw,&w,&h,&components,4);
  if (close) SDL_RWclose(rw);
  if (!pixels) { SDL_SetError("Image: %s",stbi_failure_reason()); return NULL; }
  SDL_Surface *result = SDL_CreateRGBSurfaceWithFormat(0,w,h,32,SDL_PIXELFORMAT_RGBA32);
  if (result) for (int y=0;y<h;++y)
    SDL_memcpy((Uint8*)result->pixels+y*result->pitch,pixels+(size_t)y*w*4,(size_t)w*4);
  stbi_image_free(pixels);
  return result;
}
SDL_Surface *IMG_LoadTyped_RW(SDL_RWops *rw, int close, const char *type) {
  (void)type; return IMG_Load_RW(rw,close);
}
SDL_Surface *IMG_Load(const char *path) { return IMG_Load_RW(SDL_RWFromFile(path,"rb"),1); }
SDL_Texture *IMG_LoadTexture(SDL_Renderer *renderer, const char *path) {
  SDL_Surface *s = IMG_Load(path);
  if (!s) return NULL;
  SDL_Texture *t = SDL_CreateTextureFromSurface(renderer,s);
  SDL_FreeSurface(s);
  return t;
}
