/* GPL-3.0-or-later. One renderer texture per owned pixel allocation. */
#include "pic_cache_pxa.h"
#include <stdint.h>
typedef struct Texture { struct Texture *next;const void *data;SDL_Texture *texture; } Texture;
static Texture *buckets[1024];
static Texture **slot(const void *data) {
  uintptr_t hash=((uintptr_t)data>>4)*2654435761u;
  Texture **link=&buckets[hash%1024];
  while(*link && (*link)->data!=data)link=&(*link)->next;
  return link;
}
SDL_Texture *pxa_pic_texture(const void *data) { Texture *item=*slot(data);return item?item->texture:NULL; }
void pxa_pic_release(const void *data) {
  Texture **link=slot(data),*item=*link;
  if(item){*link=item->next;SDL_DestroyTexture(item->texture);SDL_free(item);}
}
int pxa_pic_store(const void *data,SDL_Texture *texture) {
  Texture **link=slot(data);if(*link)return 0;
  Texture *item=SDL_malloc(sizeof(*item));if(!item)return 0;
  *item=(Texture){.data=data,.texture=texture};*link=item;return 1;
}
extern void __real_SDL_DestroyRenderer(SDL_Renderer *);
void __wrap_SDL_DestroyRenderer(SDL_Renderer *renderer) {
  /* Invalidate before SDL frees the renderer's textures. Pic snapshots may
   * still contain old Tex values; rendering always resolves via this cache. */
  if(renderer)for(unsigned i=0;i<1024;++i)while(buckets[i])pxa_pic_release(buckets[i]->data);
  __real_SDL_DestroyRenderer(renderer);
}
