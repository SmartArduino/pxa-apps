/* SPDX-License-Identifier: MIT. stb_image retains its own dual MIT/public-domain license. */
#include "jy_sdl.h"
#include "SDL/SDL_image.h"
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#define STBI_NO_HDR
#define STBI_NO_LINEAR
#define STBI_NO_SIMD
#define STBI_MAX_DIMENSIONS 4096
#define STB_IMAGE_IMPLEMENTATION
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#endif
#include "third_party/stb/stb_image.h"
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif
const SDL_version *IMG_Linked_Version(void){static const SDL_version version={SDL_IMAGE_MAJOR_VERSION,SDL_IMAGE_MINOR_VERSION,SDL_IMAGE_PATCHLEVEL};
 return &version;
 }
int IMG_Init(int flags){if(flags&~IMG_INIT_PNG)SDL_SetError("Only PNG image decoding is available");
 return flags&IMG_INIT_PNG;
 }
void IMG_Quit(void){}
int IMG_isPNG(SDL_RWops *rw){
 static const unsigned char signature[8]={137,80,78,71,13,10,26,10};
 if(!rw)return 0;
 int position=SDL_RWtell(rw);
 if(position<0)return 0;
 unsigned char bytes[8];
 int n=SDL_RWread(rw,bytes,1,8);
 if(SDL_RWseek(rw,position,RW_SEEK_SET)<0)return 0;
 return n==8&&!memcmp(bytes,signature,8);
}
SDL_Surface *IMG_LoadPNG_RW(SDL_RWops *rw){
 if(!rw){jy_sdl_fail("Null PNG RW input");
  return NULL;
  }int start=SDL_RWtell(rw),end=SDL_RWseek(rw,0,RW_SEEK_END);
 if(start<0||end<start||end-start>16*1024*1024||SDL_RWseek(rw,start,RW_SEEK_SET)<0){jy_sdl_fail("Invalid or oversized PNG input");
  return NULL;
  }
 int length=end-start;
 unsigned char *compressed=malloc(length? (size_t)length:1);
 if(!compressed){SDL_OutOfMemory();
  return NULL;
  }int done=0;
 while(done<length){int n=SDL_RWread(rw,compressed+done,1,length-done);
  if(n<=0){free(compressed);
   jy_sdl_fail("Truncated PNG RW data");
   return NULL;
   }done+=n;
  }
 int w=0,h=0,components=0;
 if(!stbi_info_from_memory(compressed,length,&w,&h,&components)||w<=0||h<=0||(size_t)w*(size_t)h>4u*1024u*1024u){free(compressed);
  SDL_SetError("Invalid or oversized PNG: %s",stbi_failure_reason());
  return NULL;
  }
 /* RGB images need no alpha expansion. Decode into the surface's eventual
  * pixel allocation so large title art does not occupy two full copies. */
 int channels=(components==2||components==4)?4:3;
 unsigned char *rgba=stbi_load_from_memory(compressed,length,&w,&h,&components,channels);
 free(compressed);
 if(!rgba){SDL_SetError("PNG decode failed: %s",stbi_failure_reason());
  return NULL;
  }
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
 Uint32 r=channels==4?0xff000000:0xff0000,g=channels==4?0xff0000:0xff00,b=channels==4?0xff00:0xff,a=channels==4?0xff:0;
#else
 Uint32 r=0xff,g=0xff00,b=0xff0000,a=channels==4?0xff000000:0;
#endif
 SDL_Surface *surface=SDL_CreateRGBSurfaceFrom(rgba,w,h,channels*8,w*channels,r,g,b,a);
 if(!surface){stbi_image_free(rgba);return NULL;}
 if(jy_sdl_own_pixels(surface)){SDL_FreeSurface(surface);stbi_image_free(rgba);return NULL;}
 return surface;
}
SDL_Surface *IMG_LoadTyped_RW(SDL_RWops *rw,int freesrc,char *type){
 (void)type;
 SDL_Surface *image=NULL;
 /* Build-time lossless PNG view: only one final pixel allocation is needed
  * on the Guest. The original encoded PNG is retained in the source bundle. */
 unsigned char header[16];int position=rw?SDL_RWtell(rw):-1;
 int raw=position>=0&&SDL_RWread(rw,header,1,4)==4&&!memcmp(header,"JYPI",4);
 if(position>=0)SDL_RWseek(rw,position,RW_SEEK_SET);
 if(raw){
  if(SDL_RWread(rw,header,1,16)==16){
   uint32_t values[3];for(unsigned i=0;i<3;i++){const unsigned char *p=header+4+i*4;values[i]=(uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
   uint32_t w=values[0],h=values[1],c=values[2];
   if(w&&h&&w<=4096&&h<=4096&&(uint64_t)w*h<=4194304&&(c==3||c==4)){
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
    Uint32 r=c==4?0xff000000:0xff0000,g=c==4?0xff0000:0xff00,b=c==4?0xff00:0xff,a=c==4?0xff:0;
#else
    Uint32 r=0xff,g=0xff00,b=0xff0000,a=c==4?0xff000000:0;
#endif
    image=SDL_CreateRGBSurface(0,(int)w,(int)h,(int)c*8,r,g,b,a);
    if(image)for(uint32_t y=0;y<h;y++)if(SDL_RWread(rw,(unsigned char*)image->pixels+(size_t)y*image->pitch,1,(int)(w*c))!=(int)(w*c)){SDL_FreeSurface(image);image=NULL;jy_sdl_fail("Truncated predecoded image");break;}
   }else jy_sdl_fail("Invalid predecoded image dimensions");
  }else jy_sdl_fail("Truncated predecoded image header");
 }
 else if(IMG_isPNG(rw))image=IMG_LoadPNG_RW(rw);
 else jy_sdl_fail("Only PNG image data is supported");
 if(freesrc&&rw)SDL_RWclose(rw);
 return image;
}
SDL_Surface *IMG_Load_RW(SDL_RWops *rw,int freesrc){return IMG_LoadTyped_RW(rw,freesrc,NULL);
 }
SDL_Surface *IMG_Load(const char *file){SDL_RWops *rw=SDL_RWFromFile(file,"rb");
 return rw?IMG_Load_RW(rw,1):NULL;
 }
