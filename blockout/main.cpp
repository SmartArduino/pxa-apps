/* SPDX-License-Identifier: GPL-2.0-or-later
 * Event-driven PXA frontend for the unmodified BlockOut II 2.5 game core.
 * The visibility macro exposes diagnostics only; no private game state is
 * changed by the frontend. Commands use original start and process methods.
 */
#include <new>
#include "compat/CImage.h"
#define private public
#include "vendor/Game.h"
#undef private
extern "C" {
#include "pxa_framebuffer.h"
#include "pxa_port_log.h"
}
enum { W=480,H=440,GAME_H=360,CONTROL_H=36 };
static uint8_t pixels[W*H],upload[PXA_RASTER_UPLOAD_HEADER_BYTES+W*H];
static pxfb_t fb;
static Game *game;
static SetupManager *setup;
static SoundManager *sound;
static GLFont2D *font;
static CImage *control_font;
static BYTE keys[SDLK_LAST];
static int menu=1,active=-1,frames,last_mode=-1,last_piece=-1,last_score=-1;
static uint32_t last_repeat;
static const int codes[14]={SDLK_LEFT,SDLK_RIGHT,SDLK_UP,SDLK_DOWN,'A','S','D','Q','W','E',SDLK_SPACE,'P','H',SDLK_ESCAPE};
static const char *names[14]={"LT","RT","UP","DN","X+","Y+","Z+","X-","Y-","Z-","DROP","PAUSE","HELP","MENU"};
static void trace(const char *reason){
#if PXA_RETRO_TRACE

 pxlog_t log;pxlog_init(&log);pxlog_text(&log,"BLOCKOUT ");pxlog_text(&log,reason);
 pxlog_field(&log,"menu",menu);pxlog_field(&log,"mode",game?game->gameMode:0);
 pxlog_field(&log,"piece",game?game->pIdx:0);pxlog_field(&log,"x",game?game->xPos:0);
 pxlog_field(&log,"y",game?game->yPos:0);pxlog_field(&log,"z",game?game->zPos:0);
 pxlog_field(&log,"rotate",game?game->rotateMode:0);pxlog_field(&log,"score",game?game->score.score:0);
 pxlog_field(&log,"orientation",game?(int)(game->newMatRot._11+2)*81+(int)(game->newMatRot._12+2)*27+(int)(game->newMatRot._13+2)*9+(int)(game->newMatRot._21+2)*3+(int)(game->newMatRot._22+2):0);
 pxlog_field(&log,"cubes",game?game->score.nbCube:0);pxlog_field(&log,"drop",game?game->dropMode:0);
 pxlog_field(&log,"frame",frames);pxlog_field(&log,"set",setup?setup->GetBlockSet():0);
 pxlog_field(&log,"practice",game?game->practiceFlag:0);pxlog_field(&log,"bot",game?game->demoFlag:0);
 pxlog_field(&log,"help",game?(int)(game->startShowAI*1000):0);
 pxlog_send(&log);

#else
    (void)reason;
#endif
}
static void rect(int x,int y,int w,int h,uint8_t c){
 for(int row=y;row<y+h && row<H;row++)for(int col=x;col<x+w && col<W;col++)if(row>=0&&col>=0)pixels[row*W+col]=c;
}
static void text(int x,int y,const char *s){
 const uint8_t *atlas=control_font->GetData();int aw=control_font->Width();
 while(*s){unsigned c=(unsigned char)*s++;int gx=(c%16)*16+1,gy=(c/16)*16;
  for(int row=0;row<15;row++)for(int col=0;col<9;col++)if(atlas[((gy+row)*aw+gx+col)*3+1]>80)rect(x+col,y+row,1,1,252);
  x+=9;
 }
}
static void draw(){
 bo_gl_reset();glViewport(0,0,W,GAME_H);
 if(menu){
  memset(pixels,0,W*H);rect(24,22,W-48,46,12);text(125,36,"BLOCKOUT II 2.5");
  text(61,85,"Original 41 polycubes / GPL-2+");
  const char *labels[3]={"PLAY","PRACTICE","DEMO"};
  for(int i=0;i<3;i++){rect(72,120+i*57,336,43,9);text(165,134+i*57,labels[i]);}
  const char *sets[3]={"FLAT","BASIC","EXTENDED"};
  for(int i=0;i<3;i++){rect(22+i*155,304,146,40,i==setup->GetBlockSet()?48:8);text(28+i*155,317,sets[i]);}
  text(51,369,"Pit 5x5x12 / level 0 / silent");text(29,402,"Tap a mode to start. HOME exits.");
 }else{
  game->FullRepaint();game->Render();
  rect(0,GAME_H,W,H-GAME_H,0);
  /* Original font renders into the full 440px frontend surface here. */
  glViewport(0,0,W,H);
  for(int i=0;i<14;i++){
   int x=(i%7)*W/7,y=GAME_H+(i/7)*CONTROL_H;
   rect(x+1,y+1,W/7-2,CONTROL_H-2,i==active?52:9);text(x+5,y+10,names[i]);
  }
 }
 if(pxfb_present(&fb)<0)(void)pxa_log_write(4,"BLOCKOUT frame submission failed");
}
static void start_game(int mode){
 memset(keys,0,sizeof(keys));active=-1;
 float t=fb.time_ms/1000.f+1.f;
 if(mode==1)game->StartPractice(W,GAME_H,t);
 else if(mode==2)game->StartDemo(W,GAME_H,t);
 else game->StartGame(W,GAME_H,t);
 menu=0;game->Process(keys,t);trace("start-game");
}
static void pointer(){
 if(fb.pointer_phase==PXA_POINTER_UP || fb.pointer_phase==PXA_POINTER_CANCEL){active=-1;memset(keys,0,sizeof(keys));return;}
 if(fb.pointer_phase!=PXA_POINTER_DOWN && fb.pointer_phase!=PXA_POINTER_MOVE)return;
 int x=fb.pointer_x,y=fb.pointer_y;
 if(menu){
  if(fb.pointer_phase!=PXA_POINTER_DOWN)return;
  if(x>=72&&x<408&&y>=120&&y<277){int mode=(y-120)/57;if((y-120)%57<43 && mode<3)start_game(mode);}
  else if(y>=304&&y<344){int set=(x-22)/155;if(x>=22&&set<3)setup->SetBlockSet(set);trace("setup");}
 }else{
  int next=y>=GAME_H&&y<GAME_H+2*CONTROL_H&&x>=0&&x<W?(y-GAME_H)/CONTROL_H*7+x*7/W:-1;
  if(next<0){active=-1;memset(keys,0,sizeof(keys));return;}
  if(next!=active)memset(keys,0,sizeof(keys));
  if(next>=0&&next<14 && (next!=active || fb.pointer_phase==PXA_POINTER_DOWN)){keys[codes[next]]=1;last_repeat=fb.time_ms;active=next;trace("input");}
 }
}
extern "C" int32_t pxa_app_start(const uint8_t *config,uint32_t length){
 memset(&fb,0,sizeof(fb));fb.width=W;fb.height=H;fb.pixels=pixels;fb.upload=upload;fb.upload_bytes=sizeof(upload);
 for(int i=0;i<256;i++){int r=(i>>5)&7,g=(i>>2)&7,b=i&3;fb.palette[i]=(r*31/7<<11)|(g*63/7<<5)|b*31/3;}
 fb.pointer_controller=1;int32_t status=pxfb_start(&fb,config,length,33);if(status<0)return status;
 bo_gl_init(pixels,W,GAME_H,W);
 /* The original font atlas also supplies the independent touch surface. */
 control_font=new CImage();if(!control_font->LoadImage((char*)"images/font.png"))return PXA_STATUS_INTERNAL;
 font=new GLFont2D();if(!font->RestoreDeviceObjects(W,GAME_H))return PXA_STATUS_INTERNAL;
 setup=new SetupManager();setup->SetSound(FALSE);sound=new SoundManager();
 game=new Game();game->SetSetupManager(setup);game->SetSoundManager(sound);game->pFont=font;
 glMatrixMode(GL_MODELVIEW);glLoadIdentity();gluLookAt(0,0,0,0,0,10,0,1,0);float view[16];glGetFloatv(GL_MODELVIEW_MATRIX,view);game->SetViewMatrix(view);
 menu=1;active=-1;frames=0;
 (void)pxa_log_write(2,"BLOCKOUT upstream=2.5 core=unmodified graphics=cpu-rgb332 audio=off fs=volatile input=touch");
 trace("start");return status;
}
extern "C" int32_t pxa_app_on_event(const uint8_t *event,uint32_t length){
 uint32_t kind=pxfb_event(&fb,event,length);
 if(kind&PXFB_ERROR){(void)pxa_log_write(4,"BLOCKOUT framebuffer initialization failed");return PXA_EVENT_HANDLED;}
 if(kind&PXFB_POINTER)pointer();
 if(kind&PXFB_LIFECYCLE){active=-1;memset(keys,0,sizeof(keys));}
 if(kind&PXFB_BACK){if(!menu)keys[SDLK_ESCAPE]=1;}
 if(kind&(PXFB_READY|PXFB_TICK)){
  frames++;
  if(!menu){
   if(active>=0 && active<11 && fb.time_ms-last_repeat>=180){keys[codes[active]]=1;last_repeat=fb.time_ms;}
   int pending=0;for(int i=0;i<SDLK_LAST;i++)pending|=keys[i];
   int out=game->Process(keys,fb.time_ms/1000.f+1.f);
   if(pending)trace("processed-input");
   if(out){menu=1;active=-1;memset(keys,0,sizeof(keys));trace("return-menu");}
   if(game->gameMode!=last_mode||game->pIdx!=last_piece||game->score.score!=last_score||frames%30==0){trace("tick");last_mode=game->gameMode;last_piece=game->pIdx;last_score=game->score.score;}
  }
  draw();
 }
 return kind?PXA_EVENT_HANDLED:PXA_EVENT_UNHANDLED;
}
extern "C" void pxa_app_stop(uint32_t reason){(void)reason;trace("stop");pxfb_stop(&fb);}
