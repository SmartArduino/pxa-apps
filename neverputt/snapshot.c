/* Screenshot encoder. GPL-3.0-or-later; stb implementation MIT/public domain. */
#include <stdlib.h>
#include <GL/gl.h>
#include "fs.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STBI_WRITE_NO_STDIO
#include "../common/sdl-games/vendor/stb/stb_image_write.h"
static void write_png(void *context,void *data,int bytes) {fs_write(data,1,bytes,(fs_file)context);}
void pxa_image_snapshot(const char *name) {
  GLint viewport[4];glGetIntegerv(GL_VIEWPORT,viewport);
  size_t row=(size_t)viewport[2]*4,total=row*viewport[3];
  unsigned char *pixels=malloc(total),*line=malloc(row);
  if(!pixels || !line){free(pixels);free(line);return;}
  glReadPixels(0,0,viewport[2],viewport[3],GL_RGBA,GL_UNSIGNED_BYTE,pixels);
  for(int y=0;y<viewport[3]/2;++y){
    unsigned char *a=pixels+y*row,*b=pixels+(viewport[3]-y-1)*row;
    memcpy(line,a,row);memcpy(a,b,row);memcpy(b,line,row);
  }
  fs_file file=fs_open(name,"w");
  if(file){stbi_write_png_to_func(write_png,file,viewport[2],viewport[3],4,pixels,(int)row);fs_close(file);}
  free(line);free(pixels);
}
