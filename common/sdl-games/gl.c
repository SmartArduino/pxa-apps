/* Fixed-function game pipeline on the MIT PortableGL software core.
 * GPL-3.0-or-later. No game or graphics code is linked into the Host. */
#define PORTABLEGL_IMPLEMENTATION
#include "GL/gl.h"
#include "gl_core_names.h"
#include <SDL.h>
#include <math.h>
#include "port.h"
#include "sdl_platform.h"
#include <assert.h>

static glContext context;
static pix_t *pixels;
static int initialized;
static struct {
  float matrix[3][32][16],mvp[16],normal[9];
  unsigned depth[3],mode,active,client;
  GLuint texture[3];
  float color[4],ambient[4],diffuse[4],specular[4],emission[4],shininess;
  float light_pos[8][4],light_diffuse[8][4],light_ambient[8][4],light_specular[8][4];
  float global_ambient[4],alpha_ref;
  unsigned lights,lighting,textured,alpha_test,normalize,color_material,two_side,sphere;
  GLenum alpha_func,texenv;
} state;

static void set_attribute(GLuint index,float x,float y,float z,float w) {
  context.vertex_attribs_vs[index]=(vec4){x,y,z,w};
}
static void identity(float *m) { memset(m,0,16*sizeof(float));m[0]=m[5]=m[10]=m[15]=1; }
static void multiply(float *out,const float *a,const float *b) {
  float result[16];
  for(int col=0;col<4;++col)for(int row=0;row<4;++row) {
    float v=0;for(int k=0;k<4;++k)v+=a[k*4+row]*b[col*4+k];result[col*4+row]=v;
  }
  memcpy(out,result,sizeof(result));
}
static void transform(float *out,const float *m,const float *v) {
  for(int row=0;row<4;++row)out[row]=m[row]*v[0]+m[4+row]*v[1]+m[8+row]*v[2]+m[12+row]*v[3];
}
static float *matrix(void) { return state.matrix[state.mode][state.depth[state.mode]]; }
static void normalize3(float *v) {
  float length=sqrtf(v[0]*v[0]+v[1]*v[1]+v[2]*v[2]);
  if(length>0){v[0]/=length;v[1]/=length;v[2]/=length;}
}
static float clamp01(float v) { return v<0?0:v>1?1:v; }
static void vertex_shader(float *out,vec4 *attributes,Shader_Builtins *builtins,void *uniform) {
  (void)uniform;
  float position[4]={attributes[0].x,attributes[0].y,attributes[0].z,attributes[0].w},clip[4];
  transform(clip,state.mvp,position);
  builtins->gl_Position=(vec4){clip[0],clip[1],clip[2],clip[3]};
  float color[4]={attributes[3].x,attributes[3].y,attributes[3].z,attributes[3].w};
  if(state.lighting) {
    float n[3],eye[4];
    const float source[3]={attributes[2].x,attributes[2].y,attributes[2].z};
    for(int i=0;i<3;++i)n[i]=state.normal[i]*source[0]+state.normal[3+i]*source[1]+state.normal[6+i]*source[2];
    normalize3(n);transform(eye,state.matrix[0][state.depth[0]],position);
    const float *material=state.color_material?color:state.diffuse;
    for(int c=0;c<3;++c)out[c]=state.emission[c]+state.global_ambient[c]*state.ambient[c];
    out[3]=material[3];
    for(unsigned i=0;i<8;++i)if(state.lights&(1u<<i)) {
      float light[3];for(int c=0;c<3;++c)light[c]=state.light_pos[i][c]-eye[c]*state.light_pos[i][3];
      normalize3(light);
      float diffuse=n[0]*light[0]+n[1]*light[1]+n[2]*light[2];
      diffuse=state.two_side?fabsf(diffuse):fmaxf(0,diffuse);
      float specular=0;
      if(diffuse>0 && state.shininess>0 &&
          (state.specular[0] || state.specular[1] || state.specular[2])) {
        float view[3]={-eye[0],-eye[1],-eye[2]};normalize3(view);
        float half[3]={view[0]+light[0],view[1]+light[1],view[2]+light[2]};normalize3(half);
        specular=powf(fmaxf(0,n[0]*half[0]+n[1]*half[1]+n[2]*half[2]),state.shininess);
      }
      for(int c=0;c<3;++c)out[c]+=state.ambient[c]*state.light_ambient[i][c]+
          material[c]*state.light_diffuse[i][c]*diffuse+state.specular[c]*state.light_specular[i][c]*specular;
    }
    for(int c=0;c<4;++c)out[c]=clamp01(out[c]);
  } else memcpy(out,color,sizeof(color));
  float uv[4]={attributes[1].x,attributes[1].y,0,1},transformed[4];
  if(state.sphere) {
    float n[3]={attributes[2].x,attributes[2].y,attributes[2].z};normalize3(n);
    uv[0]=n[0]*.5f+.5f;uv[1]=n[1]*.5f+.5f;
  }
  transform(transformed,state.matrix[2][state.depth[2]],uv);
  out[4]=transformed[0];out[5]=transformed[1];
}
static void fragment_shader(float *in,Shader_Builtins *builtins,void *uniform) {
  (void)uniform;
  vec4 color={in[0],in[1],in[2],in[3]};
  if(state.textured) {
    vec4 texel=texture2D(state.texture[0],in[4],in[5]);
    if(state.texenv==GL_REPLACE)color=texel;
    else {color.x*=texel.x;color.y*=texel.y;color.z*=texel.z;color.w*=texel.w;}
  }
  if(state.alpha_test &&
      ((state.alpha_func==GL_GREATER && color.w<=state.alpha_ref) ||
       (state.alpha_func==GL_GEQUAL && color.w<state.alpha_ref) ||
       (state.alpha_func==GL_LESS && color.w>=state.alpha_ref))) {
    builtins->discard=GL_TRUE;return;
  }
  builtins->gl_FragColor=color;
}
void *port_gl_create(SDL_Window *window) {
  if(initialized)return &context;
  int w,h;SDL_GetWindowSize(window,&w,&h);
  if(!init_glContext(&context,&pixels,w,h))port_error("Software GL allocation failed");
  memset(&state,0,sizeof(state));
  for(int i=0;i<3;++i)identity(state.matrix[i][0]);
  for(int c=0;c<4;++c)state.color[c]=state.diffuse[c]=1;
  for(int c=0;c<3;++c)state.ambient[c]=state.global_ambient[c]=.2f;
  state.alpha_func=GL_GREATER;state.texenv=GL_MODULATE;
  state.light_pos[0][2]=1;
  for(int c=0;c<4;++c)state.light_diffuse[0][c]=state.light_specular[0][c]=1;
  GLenum interpolation[6]={PGL_SMOOTH4,PGL_SMOOTH2};
  GLuint program=pglCreateProgram(vertex_shader,fragment_shader,6,interpolation,GL_TRUE);
  glUseProgram(program);pglSetUniform(NULL);
  set_attribute(1,0,0,0,1);set_attribute(2,0,0,1,0);set_attribute(3,1,1,1,1);
  initialized=1;port_log("PXA software GL RGB565 %dx%d",w,h);return &context;
}
void port_gl_destroy(void) { if(initialized){free_glContext(&context);initialized=0;pixels=NULL;} }
void port_gl_present(void) {
  if(initialized)port_sdl_present_rgb565(pixels,context.back_buffer.w,context.back_buffer.h,context.back_buffer.w*2);
  port_sdl_pace();
}
/* Render the 3D world at half resolution, then resolve in place before
 * full-resolution menus/HUD. The lower-left source rows are consumed before
 * any destination row can overwrite them; no allocation or extra frame copy. */
void port_gl_begin_world(void) {
  glViewport(0,0,context.back_buffer.w/2,context.back_buffer.h/2);
}
void port_gl_end_world(void) {
  int w=context.back_buffer.w,h=context.back_buffer.h;
  assert(!(w&1) && !(h&1));
  for(int y=0;y<h/2;++y){
    const uint16_t *src=pixels+(size_t)(h/2+y)*w;
    uint32_t *dest=(uint32_t*)(pixels+(size_t)y*2*w);
    for(int x=0;x<w/2;++x){uint32_t color=src[x];dest[x]=color|(color<<16);}
    port_copy_read(pixels+(size_t)(y*2+1)*w,dest,(size_t)w*2);
  }
  glViewport(0,0,w,h);
}
void port_glEnable(GLenum cap) {
  if(cap>=GL_LIGHT0 && cap<=GL_LIGHT7){state.lights|=1u<<(cap-GL_LIGHT0);return;}
  if(cap==GL_LIGHTING)state.lighting=1;
  else if(cap==GL_TEXTURE_2D)state.textured=1;
  else if(cap==GL_ALPHA_TEST)state.alpha_test=1;
  else if(cap==GL_NORMALIZE || cap==GL_RESCALE_NORMAL)state.normalize=1;
  else if(cap==GL_COLOR_MATERIAL)state.color_material=1;
  else if(cap==GL_TEXTURE_GEN_S || cap==GL_TEXTURE_GEN_T)state.sphere=1;
  else if(cap==GL_MULTISAMPLE){} /* Profile has one sample per pixel. */
  else glEnable(cap);
}
void port_glDisable(GLenum cap) {
  if(cap>=GL_LIGHT0 && cap<=GL_LIGHT7){state.lights&=~(1u<<(cap-GL_LIGHT0));return;}
  if(cap==GL_LIGHTING)state.lighting=0;
  else if(cap==GL_TEXTURE_2D)state.textured=0;
  else if(cap==GL_ALPHA_TEST)state.alpha_test=0;
  else if(cap==GL_NORMALIZE || cap==GL_RESCALE_NORMAL)state.normalize=0;
  else if(cap==GL_COLOR_MATERIAL)state.color_material=0;
  else if(cap==GL_TEXTURE_GEN_S || cap==GL_TEXTURE_GEN_T)state.sphere=0;
  else if(cap==GL_MULTISAMPLE){}
  else glDisable(cap);
}
GLboolean port_glIsEnabled(GLenum cap) {
  if(cap>=GL_LIGHT0 && cap<=GL_LIGHT7)return !!(state.lights&(1u<<(cap-GL_LIGHT0)));
  if(cap==GL_LIGHTING)return state.lighting;
  if(cap==GL_TEXTURE_2D)return state.textured;
  if(cap==GL_ALPHA_TEST)return state.alpha_test;
  if(cap==GL_NORMALIZE)return state.normalize;
  if(cap==GL_COLOR_MATERIAL)return state.color_material;
  if(cap==GL_MULTISAMPLE)return 0;
  return glIsEnabled(cap);
}
const GLubyte *port_glGetString(GLenum name) {
  if(name==GL_EXTENSIONS)return (const GLubyte*)"GL_ARB_multitexture GL_ARB_vertex_buffer_object GL_ARB_point_parameters";
  return glGetString(name);
}
void port_glGetIntegerv(GLenum name,GLint *values) {
  if(name==GL_MAX_TEXTURE_UNITS)*values=1;
  else if(name==GL_STENCIL_BITS)*values=0;
  else if(name==GL_MATRIX_MODE)*values=state.mode==0?GL_MODELVIEW:state.mode==1?GL_PROJECTION:GL_TEXTURE;
  else glGetIntegerv(name,values);
}
void port_glGetFloatv(GLenum name,GLfloat *values) {
  int mode=name==GL_MODELVIEW_MATRIX?0:name==GL_PROJECTION_MATRIX?1:name==GL_TEXTURE_MATRIX?2:-1;
  if(mode>=0)memcpy(values,state.matrix[mode][state.depth[mode]],16*sizeof(float));
  else glGetFloatv(name,values);
}
static void prepare_draw(void) {
  const float *mv=state.matrix[0][state.depth[0]];
  multiply(state.mvp,state.matrix[1][state.depth[1]],mv);
  float a[3]={mv[0],mv[1],mv[2]},b[3]={mv[4],mv[5],mv[6]},c[3]={mv[8],mv[9],mv[10]};
  for(int i=0;i<3;++i){int j=(i+1)%3,k=(i+2)%3;
    state.normal[i]=b[j]*c[k]-b[k]*c[j];state.normal[3+i]=c[j]*a[k]-c[k]*a[j];state.normal[6+i]=a[j]*b[k]-a[k]*b[j];}
  float determinant=a[0]*state.normal[0]+a[1]*state.normal[1]+a[2]*state.normal[2];
  if(determinant)for(int i=0;i<9;++i)state.normal[i]/=determinant;
}
void port_glDrawArrays(GLenum mode,GLint first,GLsizei count) {
  prepare_draw();
  const int batch=(PGL_MAX_VERTICES/3)*3;
  if(mode!=GL_TRIANGLES){assert(count<=PGL_MAX_VERTICES);glDrawArrays(mode,first,count);return;}
  for(int i=0;i<count;i+=batch)glDrawArrays(mode,first+i,count-i<batch?count-i:batch);
}
void port_glDrawElements(GLenum mode,GLsizei count,GLenum type,const void *indices) {
  prepare_draw();
  const int batch=(PGL_MAX_VERTICES/3)*3;
  if(mode!=GL_TRIANGLES){assert(count<=PGL_MAX_VERTICES);glDrawElements(mode,count,type,indices);return;}
  unsigned size=type==GL_UNSIGNED_SHORT?2:type==GL_UNSIGNED_BYTE?1:4;
  for(int i=0;i<count;i+=batch)glDrawElements(mode,count-i<batch?count-i:batch,type,(const void*)((uintptr_t)indices+(size_t)i*size));
}
void port_glMatrixMode(GLenum mode) { state.mode=mode==GL_PROJECTION?1:mode==GL_TEXTURE?2:0; }
void port_glLoadIdentity(void) { identity(matrix()); }
void port_glLoadMatrixf(const GLfloat *m) { memcpy(matrix(),m,16*sizeof(float)); }
void port_glMultMatrixf(const GLfloat *m) { multiply(matrix(),matrix(),m); }
void port_glPushMatrix(void) {
  unsigned *depth=&state.depth[state.mode];assert(*depth<31);
  memcpy(state.matrix[state.mode][*depth+1],matrix(),16*sizeof(float));++*depth;
}
void port_glPopMatrix(void) { assert(state.depth[state.mode]>0);--state.depth[state.mode]; }
void port_glTranslatef(GLfloat x,GLfloat y,GLfloat z) { float m[16];identity(m);m[12]=x;m[13]=y;m[14]=z;port_glMultMatrixf(m); }
void port_glScalef(GLfloat x,GLfloat y,GLfloat z) { float m[16];identity(m);m[0]=x;m[5]=y;m[10]=z;port_glMultMatrixf(m); }
void port_glRotatef(GLfloat angle,GLfloat x,GLfloat y,GLfloat z) {
  float axis[3]={x,y,z};normalize3(axis);x=axis[0];y=axis[1];z=axis[2];
  float c=cosf(angle*.0174532925199433f),s=sinf(angle*.0174532925199433f),t=1-c,m[16];identity(m);
  m[0]=t*x*x+c;m[4]=t*x*y-s*z;m[8]=t*x*z+s*y;
  m[1]=t*x*y+s*z;m[5]=t*y*y+c;m[9]=t*y*z-s*x;
  m[2]=t*x*z-s*y;m[6]=t*y*z+s*x;m[10]=t*z*z+c;port_glMultMatrixf(m);
}
void port_glOrtho(GLdouble l,GLdouble r,GLdouble b,GLdouble t,GLdouble n,GLdouble f) {
  float m[16];identity(m);m[0]=2/(r-l);m[5]=2/(t-b);m[10]=-2/(f-n);
  m[12]=-(r+l)/(r-l);m[13]=-(t+b)/(t-b);m[14]=-(f+n)/(f-n);port_glMultMatrixf(m);
}
void port_glColor4f(GLfloat r,GLfloat g,GLfloat b,GLfloat a) {
  state.color[0]=r;state.color[1]=g;state.color[2]=b;state.color[3]=a;set_attribute(3,r,g,b,a);
}
void port_glColor4ub(GLubyte r,GLubyte g,GLubyte b,GLubyte a) { port_glColor4f(r/255.f,g/255.f,b/255.f,a/255.f); }
void port_glVertexPointer(GLint size,GLenum type,GLsizei stride,const void *p) { glVertexAttribPointer(0,size,type,GL_FALSE,stride,p); }
void port_glTexCoordPointer(GLint size,GLenum type,GLsizei stride,const void *p) { if(!state.client)glVertexAttribPointer(1,size,type,GL_FALSE,stride,p); }
void port_glNormalPointer(GLenum type,GLsizei stride,const void *p) { glVertexAttribPointer(2,3,type,GL_FALSE,stride,p); }
void port_glColorPointer(GLint size,GLenum type,GLsizei stride,const void *p) { glVertexAttribPointer(3,size,type,GL_TRUE,stride,p); }
static int attribute(GLenum array) { return array==GL_VERTEX_ARRAY?0:array==GL_TEXTURE_COORD_ARRAY?1:array==GL_NORMAL_ARRAY?2:3; }
void port_glEnableClientState(GLenum array) { if(array!=GL_TEXTURE_COORD_ARRAY || !state.client)glEnableVertexAttribArray(attribute(array)); }
void port_glDisableClientState(GLenum array) { if(array!=GL_TEXTURE_COORD_ARRAY || !state.client)glDisableVertexAttribArray(attribute(array)); }
static float *material(GLenum name) { return name==GL_AMBIENT?state.ambient:name==GL_DIFFUSE?state.diffuse:name==GL_SPECULAR?state.specular:name==GL_EMISSION?state.emission:&state.shininess; }
void port_glMaterialfv(GLenum face,GLenum name,const GLfloat *v) {
  (void)face;if(name==GL_AMBIENT_AND_DIFFUSE){memcpy(state.ambient,v,16);memcpy(state.diffuse,v,16);}
  else memcpy(material(name),v,name==GL_SHININESS?4:16);
}
void port_glGetMaterialfv(GLenum face,GLenum name,GLfloat *v) { (void)face;memcpy(v,material(name),name==GL_SHININESS?4:16); }
void port_glLightfv(GLenum light,GLenum name,const GLfloat *v) {
  if(light<GL_LIGHT0 || light>GL_LIGHT7)return;unsigned i=light-GL_LIGHT0;
  if(name==GL_POSITION)transform(state.light_pos[i],state.matrix[0][state.depth[0]],v);
  else if(name==GL_AMBIENT)memcpy(state.light_ambient[i],v,16);
  else if(name==GL_DIFFUSE)memcpy(state.light_diffuse[i],v,16);
  else if(name==GL_SPECULAR)memcpy(state.light_specular[i],v,16);
}
void port_glLightModelfv(GLenum name,const GLfloat *v) { if(name==GL_LIGHT_MODEL_AMBIENT)memcpy(state.global_ambient,v,16); }
void port_glLightModeli(GLenum name,GLint v) { if(name==GL_LIGHT_MODEL_TWO_SIDE)state.two_side=!!v; }
void port_glLightModelf(GLenum name,GLfloat v) { port_glLightModeli(name,(GLint)v); }
void port_glAlphaFunc(GLenum function,GLclampf reference) { state.alpha_func=function;state.alpha_ref=reference; }
void port_glTexEnvi(GLenum target,GLenum name,GLint value) { if(target==GL_TEXTURE_ENV && name==GL_TEXTURE_ENV_MODE)state.texenv=value; }
void port_glTexGeni(GLenum coord,GLenum name,GLint value) { (void)coord;(void)name;assert(value==GL_SPHERE_MAP); }
void port_glActiveTexture(GLenum unit) { state.active=unit-GL_TEXTURE0;assert(state.active<3); }
void port_glClientActiveTexture(GLenum unit) { state.client=unit-GL_TEXTURE0;assert(state.client<3); }
void port_glBindTexture(GLenum target,GLuint texture) { state.texture[state.active]=texture;glBindTexture(target,texture); }
void port_glClipPlane(GLenum plane,const GLdouble *equation) { (void)plane;(void)equation;port_error("Clip planes are disabled in this renderer profile"); }
void port_glPointParameterf(GLenum name,GLfloat value) { (void)name;(void)value; }
void port_glPointParameterfv(GLenum name,const GLfloat *values) { (void)name;(void)values; }
GLboolean port_glIsBuffer(GLuint buffer) { return buffer<context.buffers.size && !context.buffers.a[buffer].deleted; }

int __wrap_SDL_GL_SetAttribute(SDL_GLattr attribute,int value) { (void)attribute;(void)value;return 0; }
int __wrap_SDL_GL_GetAttribute(SDL_GLattr attribute,int *value) {
  *value=attribute==SDL_GL_RED_SIZE?5:attribute==SDL_GL_GREEN_SIZE?6:attribute==SDL_GL_BLUE_SIZE?5:attribute==SDL_GL_DEPTH_SIZE?16:attribute==SDL_GL_DOUBLEBUFFER?1:0;return 0;
}
SDL_GLContext __wrap_SDL_GL_CreateContext(SDL_Window *window) { return port_gl_create(window); }
void __wrap_SDL_GL_DeleteContext(SDL_GLContext context_) { (void)context_;port_gl_destroy(); }
int __wrap_SDL_GL_MakeCurrent(SDL_Window *window,SDL_GLContext ctx) { (void)window;return ctx==&context?0:-1; }
int __wrap_SDL_GL_SetSwapInterval(int interval) { (void)interval;return 0; }
void __wrap_SDL_GL_GetDrawableSize(SDL_Window *window,int *w,int *h) { SDL_GetWindowSize(window,w,h); }
void __wrap_SDL_GL_SwapWindow(SDL_Window *window) { (void)window;port_gl_present(); }
void *__wrap_SDL_GL_GetProcAddress(const char *name) {
#define PROC(label,function) if(!strcmp(name,label))return (void*)(uintptr_t)&function
  PROC("glActiveTextureARB",port_glActiveTexture);PROC("glClientActiveTextureARB",port_glClientActiveTexture);
  PROC("glGenBuffersARB",glGenBuffers);PROC("glBindBufferARB",glBindBuffer);
  PROC("glBufferDataARB",glBufferData);PROC("glBufferSubDataARB",glBufferSubData);
  PROC("glDeleteBuffersARB",glDeleteBuffers);PROC("glIsBufferARB",port_glIsBuffer);
  PROC("glPointParameterfARB",port_glPointParameterf);PROC("glPointParameterfvARB",port_glPointParameterfv);
#undef PROC
  return NULL;
}
