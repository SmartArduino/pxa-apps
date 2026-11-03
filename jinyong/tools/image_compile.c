/* SPDX-License-Identifier: MIT. Build-time decoder; stb_image retains its license. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#define STBI_ONLY_PNG
#define STBI_NO_HDR
#define STBI_NO_LINEAR
#define STBI_NO_SIMD
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
static void put32(FILE *f,uint32_t n){for(unsigned i=0;i<4;i++)fputc((int)((n>>(i*8))&255),f);}
int main(int argc,char **argv){
 if(argc!=3)return 2;int w,h,n;if(!stbi_info(argv[1],&w,&h,&n)||w<=0||h<=0||w>4096||h>4096||(uint64_t)w*h>4194304)return 3;
 int channels=(n==2||n==4)?4:3;unsigned char *pixels=stbi_load(argv[1],&w,&h,&n,channels);if(!pixels){fprintf(stderr,"%s\n",stbi_failure_reason());return 4;}
 FILE *f=fopen(argv[2],"wb");if(!f){stbi_image_free(pixels);return 5;}fwrite("JYPI",1,4,f);put32(f,(uint32_t)w);put32(f,(uint32_t)h);put32(f,(uint32_t)channels);
 size_t bytes=(size_t)w*h*channels;int ok=fwrite(pixels,1,bytes,f)==bytes;stbi_image_free(pixels);if(fclose(f))ok=0;return ok?0:6;
}
