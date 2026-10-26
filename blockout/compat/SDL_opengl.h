/* SPDX-License-Identifier: GPL-2.0-or-later
 * Port-local CPU subset of the OpenGL 1.x calls used by BlockOut II.
 * Matrix operations follow OpenGL column-major, postmultiply semantics.
 */
#ifndef PXA_BLOCKOUT_GL_H
#define PXA_BLOCKOUT_GL_H
#include <stdint.h>
typedef float GLfloat; typedef double GLdouble; typedef unsigned GLenum;
typedef unsigned GLuint; typedef int GLint; typedef int GLsizei;
enum { GL_MODELVIEW=1, GL_PROJECTION, GL_MODELVIEW_MATRIX, GL_PROJECTION_MATRIX,
 GL_LINES, GL_TRIANGLES, GL_QUADS, GL_COMPILE, GL_TEXTURE_2D, GL_BLEND,
 GL_LIGHTING, GL_DEPTH_TEST, GL_CULL_FACE, GL_FRONT, GL_BACK,
 GL_TEXTURE_MIN_FILTER, GL_TEXTURE_MAG_FILTER, GL_LINEAR, GL_NEAREST,
 GL_RGB, GL_RGBA, GL_UNSIGNED_BYTE, GL_FLOAT, GL_SRC_ALPHA,
 GL_ONE_MINUS_SRC_ALPHA, GL_DEPTH_BUFFER_BIT=0x100, GL_COLOR_BUFFER_BIT=0x200,
 GL_NO_ERROR=0 };
void glMatrixMode(GLenum); void glLoadIdentity(); void glLoadMatrixf(const GLfloat*);
void glMultMatrixf(const GLfloat*); void glGetFloatv(GLenum,GLfloat*);
void glPushMatrix(); void glPopMatrix(); void glTranslatef(float,float,float);
void glOrtho(double,double,double,double,double,double);
void gluPerspective(double,double,double,double);
void gluLookAt(double,double,double,double,double,double,double,double,double);
void glViewport(int,int,int,int); void glClear(GLenum);
void glEnable(GLenum); void glDisable(GLenum); void glCullFace(GLenum);
void glBlendFunc(GLenum,GLenum); void glColor3f(float,float,float);
void glColor4f(float,float,float,float); void glBegin(GLenum); void glEnd();
void glNormal3f(float,float,float); void glTexCoord2f(float,float);
void glVertex3f(float,float,float); void glVertex2i(int,int);
GLuint glGenLists(int); void glNewList(GLuint,GLenum); void glEndList();
void glCallList(GLuint); void glDeleteLists(GLuint,int);
void glGenTextures(int,GLuint*); void glDeleteTextures(int,const GLuint*);
void glBindTexture(GLenum,GLuint); void glTexParameteri(GLenum,GLenum,GLint);
void glTexImage2D(GLenum,int,int,int,int,int,GLenum,GLenum,const void*);
GLenum glGetError(); void glRasterPos2i(int,int);
void glDrawPixels(int,int,GLenum,GLenum,const void*);
void bo_gl_init(uint8_t*,int,int,int); void bo_gl_reset();
void bo_gl_material(float,float,float,float);
#endif
