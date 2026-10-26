/* SPDX-License-Identifier: GPL-2.0-or-later
 * Small CPU renderer for the fixed-function subset actually used by upstream.
 * Original display lists, vertices, view/projection matrices and draw ordering
 * remain in charge of the scene. Pixels use RGB332 then public PXA INDEX8.
 */
#include "SDL_opengl.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <algorithm>
static uint8_t *pixels;
static int width,height,stride,vx,vy,vw,vh;
static float *depth;
static float model[16],projection[16],stack[32][16];
static int stackpos,which,depth_on,texture_on,blend_on,cull_on,cull_face;
static float rgba[4]={1,1,1,1},uv[2];
static unsigned next_list=1,recording,next_tex=1,bound_tex;
static GLenum primitive;
struct Command { int op; float a,b,c; };
struct List { Command *commands; unsigned used,capacity; int allocated; };
static List lists[512];
struct Texture { int w,h; uint8_t *rgb,*alpha; int allocated; };
static Texture textures[128];
struct Vertex { float x,y,z,u,v; int valid; };
static Vertex vertices[4];
static int vertex_count;
static int clampi(int a,int lo,int hi){return a<lo?lo:a>hi?hi:a;}
static uint8_t color(float r,float g,float b){
 return (clampi((int)(r*7+0.5f),0,7)<<5)|(clampi((int)(g*7+0.5f),0,7)<<2)|clampi((int)(b*3+0.5f),0,3);
}
static void identity(float *a){memset(a,0,64);a[0]=a[5]=a[10]=a[15]=1;}
static float *matrix(){return which==GL_PROJECTION?projection:model;}
static void multiply(float *a,const float *b){
 float c[16];
 for(int col=0;col<4;col++)for(int row=0;row<4;row++){
  c[col*4+row]=0;for(int k=0;k<4;k++)c[col*4+row]+=a[k*4+row]*b[col*4+k];
 }
 memcpy(a,c,64);
}
void glMatrixMode(GLenum n){which=n;}
void glLoadIdentity(){identity(matrix());}
void glLoadMatrixf(const float *m){memcpy(matrix(),m,64);}
void glMultMatrixf(const float *m){multiply(matrix(),m);}
void glGetFloatv(GLenum n,float *out){memcpy(out,n==GL_PROJECTION_MATRIX?projection:model,64);}
void glPushMatrix(){if(stackpos<32)memcpy(stack[stackpos++],matrix(),64);}
void glPopMatrix(){if(stackpos)memcpy(matrix(),stack[--stackpos],64);}
void glTranslatef(float x,float y,float z){float m[16];identity(m);m[12]=x;m[13]=y;m[14]=z;glMultMatrixf(m);}
void glOrtho(double l,double r,double b,double t,double n,double f){
 float m[16]={0};m[0]=2/(r-l);m[5]=2/(t-b);m[10]=-2/(f-n);m[15]=1;
 m[12]=-(r+l)/(r-l);m[13]=-(t+b)/(t-b);m[14]=-(f+n)/(f-n);glMultMatrixf(m);
}
void gluPerspective(double angle,double aspect,double n,double f){
 float m[16]={0};float k=1/tanf((float)angle*0.00872664625997165f);
 m[0]=k/aspect;m[5]=k;m[10]=(f+n)/(n-f);m[11]=-1;m[14]=2*f*n/(n-f);glMultMatrixf(m);
}
void gluLookAt(double ex,double ey,double ez,double cx,double cy,double cz,double ux,double uy,double uz){
 float fx=cx-ex,fy=cy-ey,fz=cz-ez;float len=sqrtf(fx*fx+fy*fy+fz*fz);fx/=len;fy/=len;fz/=len;
 float sx=fy*uz-fz*uy,sy=fz*ux-fx*uz,sz=fx*uy-fy*ux;len=sqrtf(sx*sx+sy*sy+sz*sz);sx/=len;sy/=len;sz/=len;
 float tx=sy*fz-sz*fy,ty=sz*fx-sx*fz,tz=sx*fy-sy*fx;
 float m[16]={sx,tx,-fx,0,sy,ty,-fy,0,sz,tz,-fz,0,0,0,0,1};
 glMultMatrixf(m);glTranslatef(-ex,-ey,-ez);
}
void glViewport(int x,int y,int w,int h){vx=x;vy=y;vw=w;vh=h;}
void glClear(GLenum mask){if(mask&GL_DEPTH_BUFFER_BIT)for(int i=0;i<width*height;i++)depth[i]=1e20f;if(mask&GL_COLOR_BUFFER_BIT)memset(pixels,0,stride*height);}
void glEnable(GLenum c){if(c==GL_DEPTH_TEST)depth_on=1;else if(c==GL_TEXTURE_2D)texture_on=1;else if(c==GL_BLEND)blend_on=1;else if(c==GL_CULL_FACE)cull_on=1;}
void glDisable(GLenum c){if(c==GL_DEPTH_TEST)depth_on=0;else if(c==GL_TEXTURE_2D)texture_on=0;else if(c==GL_BLEND)blend_on=0;else if(c==GL_CULL_FACE)cull_on=0;}
void glCullFace(GLenum c){cull_face=c;}
void glBlendFunc(GLenum,GLenum){}
void glColor4f(float r,float g,float b,float a){rgba[0]=r;rgba[1]=g;rgba[2]=b;rgba[3]=a;}
void glColor3f(float r,float g,float b){glColor4f(r,g,b,1);}
void bo_gl_material(float r,float g,float b,float a){glColor4f(r,g,b,a);}
static void record(int op,float a=0,float b=0,float c=0){
 List *list=&lists[recording];
 if(list->used==list->capacity){list->capacity=list->capacity?list->capacity*2:64;list->commands=(Command*)realloc(list->commands,list->capacity*sizeof(Command));}
 list->commands[list->used++]={op,a,b,c};
}
static Vertex transform(float x,float y,float z){
 float in[4]={x,y,z,1},eye[4]={0},clip[4]={0};
 for(int row=0;row<4;row++)for(int k=0;k<4;k++)eye[row]+=model[k*4+row]*in[k];
 for(int row=0;row<4;row++)for(int k=0;k<4;k++)clip[row]+=projection[k*4+row]*eye[k];
 Vertex v;v.valid=clip[3]>0.00001f;
 if(!v.valid){v.x=v.y=v.z=0;return v;}
 v.x=vx+(clip[0]/clip[3]+1)*vw*0.5f;
 v.y=height-(vy+(clip[1]/clip[3]+1)*vh*0.5f);
 v.z=clip[2]/clip[3];v.u=uv[0];v.v=uv[1];return v;
}
static void put(int x,int y,float z,uint8_t c,float alpha){
 if(!blend_on)alpha=1;
 /* Clip to the current viewport. Original pit faces can extend beyond NDC;
  * OpenGL clips them before rasterizing so they must not cover the HUD. */
 if(x<0||y<0||x>=width||y>=height||x<vx||x>=vx+vw||
    y<height-vy-vh||y>=height-vy||alpha<=0)return;
 int d=y*width+x,p=y*stride+x;
 if(depth_on && z>depth[d]+0.00001f)return;
 if(depth_on)depth[d]=z;
 if(blend_on && alpha<0.99f){
  uint8_t old=pixels[p];float r=((c>>5)&7)/7.f,g=((c>>2)&7)/7.f,b=(c&3)/3.f;
  pixels[p]=color(r*alpha+((old>>5)&7)/7.f*(1-alpha),g*alpha+((old>>2)&7)/7.f*(1-alpha),b*alpha+(old&3)/3.f*(1-alpha));
 }else pixels[p]=c;
}
static float edge(const Vertex &a,const Vertex &b,float x,float y){return (b.x-a.x)*(y-a.y)-(b.y-a.y)*(x-a.x);}
static void triangle(Vertex a,Vertex b,Vertex c){
 if(!a.valid||!b.valid||!c.valid)return;
 float area=edge(a,b,c.x,c.y);if(fabsf(area)<0.01f)return;
 if(cull_on && ((cull_face==GL_FRONT && area<0)||(cull_face!=GL_FRONT && area>0)))return;
 int x0=clampi((int)floorf(std::min(a.x,std::min(b.x,c.x))),0,width-1);
 int x1=clampi((int)ceilf(std::max(a.x,std::max(b.x,c.x))),0,width-1);
 int y0=clampi((int)floorf(std::min(a.y,std::min(b.y,c.y))),0,height-1);
 int y1=clampi((int)ceilf(std::max(a.y,std::max(b.y,c.y))),0,height-1);
 Texture *t=texture_on && bound_tex<128?&textures[bound_tex]:0;
 uint8_t flat=color(rgba[0],rgba[1],rgba[2]);
 for(int y=y0;y<=y1;y++)for(int x=x0;x<=x1;x++){
  float wa=edge(b,c,x+.5f,y+.5f)/area,wb=edge(c,a,x+.5f,y+.5f)/area,wc=1-wa-wb;
  if(wa<0||wb<0||wc<0)continue;
  uint8_t out=flat;float alpha=rgba[3];
  if(t && t->rgb){
   float u=wa*a.u+wb*b.u+wc*c.u,v=wa*a.v+wb*b.v+wc*c.v;
   int tx=clampi((int)(u*t->w),0,t->w-1),ty=clampi((int)(v*t->h),0,t->h-1),pos=ty*t->w+tx;
   uint8_t tc=t->rgb[pos];out=color(((tc>>5)&7)/7.f*rgba[0],((tc>>2)&7)/7.f*rgba[1],(tc&3)/3.f*rgba[2]);
   if(t->alpha)alpha*=t->alpha[pos]/255.f;
  }
  put(x,y,wa*a.z+wb*b.z+wc*c.z,out,alpha);
 }
}
static void line(Vertex a,Vertex b){
 if(!a.valid||!b.valid)return;
 int steps=(int)ceilf(std::max(fabsf(b.x-a.x),fabsf(b.y-a.y)));if(steps>4096)return;if(steps<1)steps=1;
 uint8_t c=color(rgba[0],rgba[1],rgba[2]);
 for(int i=0;i<=steps;i++){float t=(float)i/steps;put((int)(a.x+(b.x-a.x)*t),(int)(a.y+(b.y-a.y)*t),a.z+(b.z-a.z)*t,c,rgba[3]);}
}
void glBegin(GLenum p){if(recording){record(1,p);return;}primitive=p;vertex_count=0;}
void glEnd(){if(recording)record(2);vertex_count=0;}
void glNormal3f(float,float,float){} /* Ambient-only shading; geometry is unchanged. */
void glTexCoord2f(float u,float v){if(recording){record(3,u,v);return;}uv[0]=u;uv[1]=v;}
void glVertex3f(float x,float y,float z){
 if(recording){record(4,x,y,z);return;}
 vertices[vertex_count++]=transform(x,y,z);
 if(primitive==GL_LINES && vertex_count==2){line(vertices[0],vertices[1]);vertex_count=0;}
 else if(primitive==GL_TRIANGLES && vertex_count==3){triangle(vertices[0],vertices[1],vertices[2]);vertex_count=0;}
 else if(primitive==GL_QUADS && vertex_count==4){triangle(vertices[0],vertices[1],vertices[2]);triangle(vertices[0],vertices[2],vertices[3]);vertex_count=0;}
}
void glVertex2i(int x,int y){glVertex3f(x,y,0);}
GLuint glGenLists(int n){for(unsigned id=1;id+(unsigned)n<=512;id++){int free=1;for(int i=0;i<n;i++)if(lists[id+i].allocated)free=0;if(free){for(int i=0;i<n;i++)lists[id+i].allocated=1;return id;}}return 0;}
void glNewList(GLuint id,GLenum){if(id && id<512){recording=id;lists[id].used=0;}}
void glEndList(){recording=0;}
void glCallList(GLuint id){
 if(!id||id>=512)return;
 List *l=&lists[id];for(unsigned i=0;i<l->used;i++){Command *c=&l->commands[i];switch(c->op){case 1:glBegin(c->a);break;case 2:glEnd();break;case 3:glTexCoord2f(c->a,c->b);break;case 4:glVertex3f(c->a,c->b,c->c);break;}}
}
void glDeleteLists(GLuint id,int n){for(int i=0;i<n && id+i<512;i++){free(lists[id+i].commands);lists[id+i]={0};}if(id+(unsigned)n==next_list)next_list=id;}
void glGenTextures(int n,GLuint *out){for(int i=0;i<n;i++){out[i]=0;for(unsigned id=1;id<128;id++)if(!textures[id].allocated){textures[id].allocated=1;out[i]=id;break;}}}
void glDeleteTextures(int n,const GLuint *ids){for(int i=0;i<n;i++)if(ids[i]<128){Texture *t=&textures[ids[i]];free(t->rgb);free(t->alpha);*t={0};}}
void glBindTexture(GLenum,GLuint id){bound_tex=id;}
void glTexParameteri(GLenum,GLenum,GLint){} /* Documented nearest sampling on INDEX8. */
void glTexImage2D(GLenum,int,int,int w,int h,int,GLenum format,GLenum,const void *data){
 if(!bound_tex||bound_tex>=128)return;Texture *t=&textures[bound_tex];free(t->rgb);free(t->alpha);t->w=w;t->h=h;
 t->rgb=(uint8_t*)malloc(w*h);t->alpha=format==GL_RGBA?(uint8_t*)malloc(w*h):0;const uint8_t *p=(const uint8_t*)data;int bpp=format==GL_RGBA?4:3;
 for(int i=0;i<w*h;i++){t->rgb[i]=color(p[i*bpp]/255.f,p[i*bpp+1]/255.f,p[i*bpp+2]/255.f);if(t->alpha)t->alpha[i]=p[i*bpp+3];}
}
GLenum glGetError(){return GL_NO_ERROR;}
static int rasterx,rastery;
void glRasterPos2i(int x,int y){Vertex v=transform(x,y,0);rasterx=v.x;rastery=v.y;}
void glDrawPixels(int w,int h,GLenum,GLenum type,const void *data){
 if(type!=GL_FLOAT)return;const float *p=(const float*)data;
 for(int y=0;y<h;y++)for(int x=0;x<w;x++){int i=(y*w+x)*4;put(rasterx+x,rastery-y-1,0,color(p[i],p[i+1],p[i+2]),p[i+3]);}
}
void bo_gl_init(uint8_t *p,int w,int h,int s){pixels=p;width=w;height=h;stride=s;depth=(float*)malloc(w*h*sizeof(float));which=GL_MODELVIEW;identity(model);identity(projection);glViewport(0,0,w,h);glClear(GL_DEPTH_BUFFER_BIT);}
void bo_gl_reset(){glClear(GL_DEPTH_BUFFER_BIT);}
