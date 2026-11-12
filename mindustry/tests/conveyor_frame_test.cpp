// SPDX-License-Identifier: GPL-3.0-only
#define MINDUSTRY_UI_SUPPORT_ONLY
#include "ui_test.cpp"
int main(){
 CHECK(gfx::attach(1,PXA_RASTER_CAP_TEXTURED_QUAD|PXA_RASTER_CAP_PAINTER_POLYGON|PXA_RASTER_CAP_AFFINE_UV|PXA_RASTER_CAP_SPRITE_TEXEL_ALPHA));
 pocket::WIDTH=pocket::HEIGHT=480;milky::screen={480,480,{0,0,480,480}};
 for(int zoomValue:{8,12,16,24,32,40})for(int directionValue=0;directionValue<4;++directionValue){
  s.generate(154,0,true);for(auto &tile:s.tiles)tile={Stone,None};for(auto &b:s.buildings)if(b.type!=Core)b={};s.rebuild();
  int dx=directionValue==0?1:directionValue==2?-1:0,dy=directionValue==1?-1:directionValue==3?1:0;
  for(int k=0;k<5;++k)CHECK(s.build(Conveyor,24+k*dx,26+k*dy,directionValue,true)>=0);
  resetUI();view=World;zoom=zoomValue;cameraX=24.5f;cameraY=27;
  CHECK(gfx::begin(1,480,480));worldDraw();CHECK(gfx::finish()>0);
  bool body=false;unsigned shadows=0;
  for(unsigned at=32;at<frame.size();at+=pxa_load_u16(frame.data()+at+2)){
   auto kind=frame[at];
   if(kind==PXA_RASTER_RECORD_FLAT_QUAD && pxa_load_u16(frame.data()+at+4)==rgb(28,28,33)){
    CHECK(!body);++shadows;
   }
   if((kind==PXA_RASTER_RECORD_SPRITE || kind==PXA_RASTER_RECORD_SPRITE_BATCH || kind==PXA_RASTER_RECORD_TEXTURED_QUAD) && frame[at+4]==0)body=true;
  }
  CHECK(body && shadows>=3);
 }
 for(int shape=0;shape<5;++shape)for(int phase=0;phase<4;++phase)for(int dir=0;dir<4;++dir)for(bool flip:{false,true}){
  const auto &sprite=*art::belt_frames[shape][phase];CHECK(sprite.w==16 && sprite.h==16);
  CHECK(gfx::begin(1,480,480));gfx::sprite(sprite,32,32,32,32,dir,255,0,flip);CHECK(gfx::finish()>0);
  CHECK(pxa_load_u32(frame.data()+16)==1);
 }
 gfx::detach();std::printf("%d conveyor frame checks passed: shadow ordering, four orientations, five shapes, mirrors, four phases and six zooms\n",checks);
}
