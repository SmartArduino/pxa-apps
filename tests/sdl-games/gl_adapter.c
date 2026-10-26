/* Fixed-pipeline regression: RGB565, alpha blend, RGBA textures, matrices. */
#include <SDL.h>
#include <GL/gl.h>
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
void port_log(const char *format,...) {(void)format;}
void port_error(const char *message) {fprintf(stderr,"%s\n",message);abort();}
void port_sdl_present_rgb565(const uint16_t *p,int w,int h,int pitch) {(void)p;(void)w;(void)h;(void)pitch;}
void port_sdl_pace(void) {}
extern void *port_gl_create(SDL_Window *);
extern void port_gl_destroy(void);
extern void port_gl_begin_world(void);
extern void port_gl_end_world(void);
static void expect(unsigned r,unsigned g,unsigned b) {
  unsigned char rgba[4]={0};glReadPixels(16,16,1,1,GL_RGBA,GL_UNSIGNED_BYTE,rgba);
  assert(abs((int)rgba[0]-(int)r)<10);
  assert(abs((int)rgba[1]-(int)g)<10);
  assert(abs((int)rgba[2]-(int)b)<10);
  assert(glGetError()==GL_NO_ERROR);
}
int main(void) {
  SDL_SetHint(SDL_HINT_VIDEODRIVER,"dummy");assert(SDL_Init(SDL_INIT_VIDEO)==0);
  SDL_Window *window=SDL_CreateWindow("test",0,0,32,32,0);assert(window);assert(port_gl_create(window));
  glViewport(0,0,32,32);glMatrixMode(GL_PROJECTION);glLoadIdentity();glMatrixMode(GL_MODELVIEW);glLoadIdentity();
  float pos[]={-1,-1,0, 1,-1,0, 1,1,0, -1,-1,0, 1,1,0, -1,1,0};
  float uv[]={0,0,1,0,1,1,0,0,1,1,0,1};
  glVertexPointer(3,GL_FLOAT,0,pos);glEnableClientState(GL_VERTEX_ARRAY);
  glClearColor(0,0,1,1);glClear(GL_COLOR_BUFFER_BIT);expect(0,0,255);
  glColor4f(1,0,0,1);glDrawArrays(GL_TRIANGLES,0,6);expect(255,0,0);
  glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
  glColor4f(0,1,0,.5f);glDrawArrays(GL_TRIANGLES,0,6);expect(127,127,0);
  glClearColor(0,0,1,1);glClear(GL_COLOR_BUFFER_BIT);
  unsigned char texel[]={255,0,0,128};GLuint tex;
  glGenTextures(1,&tex);glBindTexture(GL_TEXTURE_2D,tex);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,1,1,0,GL_RGBA,GL_UNSIGNED_BYTE,texel);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
  glEnable(GL_TEXTURE_2D);glTexCoordPointer(2,GL_FLOAT,0,uv);glEnableClientState(GL_TEXTURE_COORD_ARRAY);
  glColor4f(1,1,1,1);glDrawArrays(GL_TRIANGLES,0,6);expect(128,0,127);
  glDeleteTextures(1,&tex);
  glDisable(GL_TEXTURE_2D);glDisable(GL_BLEND);glColor4f(1,0,0,1);
  glClearColor(0,0,1,1);glClear(GL_COLOR_BUFFER_BIT);
  port_gl_begin_world();glDrawArrays(GL_TRIANGLES,0,6);port_gl_end_world();
  unsigned char image[32*32*4];glReadPixels(0,0,32,32,GL_RGBA,GL_UNSIGNED_BYTE,image);
  for(unsigned i=0;i<32*32;++i){assert(image[i*4]>245);assert(image[i*4+1]<10);assert(image[i*4+2]<10);}
  port_gl_destroy();SDL_DestroyWindow(window);SDL_Quit();puts("GL adapter colors and alpha passed");
}
