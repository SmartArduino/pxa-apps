/* SPDX-License-Identifier: GPL-2.0-or-later
 * SDL window and mixer boundary. No sound capability is requested.
 */
#include "Game.h"
#include "CImage.h"
#include "../assets.h"
int CImage::LoadImage(char *name) {
  while(*name=='/') ++name;
  for(int i=0;i<BO_IMAGE_COUNT;i++) if(!strcmp(name,bo_images[i].name)){index=i;return 1;}
  index=-1; return 0;
}
int CImage::Width(){return index<0?0:bo_images[index].width;}
int CImage::Height(){return index<0?0:bo_images[index].height;}
unsigned char *CImage::GetData(){return index<0?0:(unsigned char*)bo_images[index].data;}
void GLApplication::SetMaterial(GLMATERIAL *m){bo_gl_material(m->Ambient.r,m->Ambient.g,m->Ambient.b,m->Ambient.a);}
SoundManager::SoundManager(){enabled=FALSE;}
int SoundManager::Create(){return GL_OK;}
void SoundManager::SetEnable(BOOL e){enabled=e;}
char *SoundManager::GetErrorMsg(){return (char*)"silent PXA frontend";}
#define SILENT(name) void SoundManager::name(){}
SILENT(PlayBlub) SILENT(PlayWozz) SILENT(PlayTchh) SILENT(PlayLine)
SILENT(PlayLevel) SILENT(PlayEmpty) SILENT(PlayWellDone) SILENT(PlayLine2)
SILENT(PlayLevel2) SILENT(PlayEmpty2) SILENT(PlayWellDone2) SILENT(PlayHit)
SILENT(PlayMusic) SILENT(StopMusic)
