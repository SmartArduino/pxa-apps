// SPDX-License-Identifier: GPL-3.0-only
#define MINDUSTRY_UI_SUPPORT_ONLY
#include "ui_test.cpp"
#include "../render_assets.hpp"
const md_art::Region &regionFor(const art::Sprite &sprite){
 for(const auto &r:md_art::regions)if(r.sprite==&sprite)return r;
 CHECK(false);return md_art::regions[0];
}
void checkImageAt(const art::Sprite &sprite,int x,int y,int size,bool quad){
 auto &r=regionFor(sprite);bool found=false;
 for(unsigned at=32;at<frame.size();at+=pxa_load_u16(frame.data()+at+2)){
  const auto *p=frame.data()+at;
  if(p[4]!=0)continue;
  if(!quad && p[0]==PXA_RASTER_RECORD_SPRITE && int16_t(pxa_load_u16(p+8))==x && int16_t(pxa_load_u16(p+10))==y && pxa_load_u16(p+12)==size && pxa_load_u16(p+14)==size){
   CHECK(pxa_load_u16(p+16)==r.x && pxa_load_u16(p+18)==r.y);found=true;
  }
  if(quad && p[0]==PXA_RASTER_RECORD_TEXTURED_QUAD && int16_t(pxa_load_u16(p+8))==x*16 && int16_t(pxa_load_u16(p+10))==y*16){
   int uv[4][2]={{r.x,r.y},{r.x+r.w,r.y},{r.x+r.w,r.y+r.h},{r.x,r.y+r.h}};
   for(int i=0;i<4;++i){CHECK(int16_t(pxa_load_u16(p+8+i*12+4))==uv[i][0]*16);CHECK(int16_t(pxa_load_u16(p+8+i*12+6))==uv[i][1]*16);}
   found=true;
  }
 }
 CHECK(found);
}
int main(int argc,char **argv){
 CHECK(gfx::attach(1,PXA_RASTER_CAP_TEXTURED_QUAD|PXA_RASTER_CAP_PAINTER_POLYGON|PXA_RASTER_CAP_AFFINE_UV|PXA_RASTER_CAP_SPRITE_TEXEL_ALPHA));
 pocket::WIDTH=pocket::HEIGHT=480;milky::screen={480,480,{0,0,480,480}};
 if(argc>1){
  s.generate(154,0,true);for(auto &b:s.buildings)if(b.type!=Core)b={};s.rebuild();
  s.playerX=s.targetX=26.5f;s.playerY=s.targetY=18.5f;
  for(int i=0;i<8;++i)CHECK(s.build(i<4?GroundFactory:AirFactory,23,3+i*4,i%4,true)>=0);
  std::array<uint8_t,3900> bytes{};auto n=s.save(bytes.data(),bytes.size());CHECK(n);auto *f=std::fopen(argv[1],"wb");CHECK(f);CHECK(std::fwrite(bytes.data(),1,n,f)==n);std::fclose(f);return 0;
 }
 for(int block=Core;block<Blocks;++block){
  const auto &im=blockArt(block);CHECK(im.w>0 && im.h>0);
  CHECK(gfx::begin(1,480,480));gfx::sprite(im,100,100,48,48);CHECK(gfx::finish()>0);CHECK(pxa_load_u32(frame.data()+16)==1);
 }
 for(auto type:{GroundFactory,AirFactory})for(int dir=0;dir<4;++dir){
  const auto &im=*(type==GroundFactory?art::groundfactory_frames:art::airfactory_frames)[dir];CHECK(&blockArt(type,dir)==&im);
  s.generate(154,0,true);for(auto &tile:s.tiles)tile={Stone,None};for(auto &b:s.buildings)if(b.type!=Core)b={};s.rebuild();
  int id=s.build(type,24,28,dir,true);CHECK(id>=0);resetUI();view=World;zoom=16;cameraX=24.5f;cameraY=30;
  int x=originX()+24*zoom,y=originY()+28*zoom;
  CHECK(gfx::begin(1,480,480));worldDraw();CHECK(gfx::finish()>0);checkImageAt(im,x,y,48,false);
  // Construction uses the same composed direction without rotating the base.
  s.buildings[id].buildState=1;CHECK(gfx::begin(1,480,480));worldDraw();CHECK(gfx::finish()>0);checkImageAt(im,x,y,48,true);
  s.buildings[id]={};s.rebuild();selected=type;direction=dir;previewX=24;previewY=28;
  CHECK(gfx::begin(1,480,480));game::draw();CHECK(gfx::finish()>0);checkImageAt(im,x,y,48,true);
 }
 CHECK(glyphRequests==0);gfx::detach();std::printf("%d building frame checks passed: all 22 icons, factory four-way world/blueprint/preview layers and font coverage\n",checks);
}
