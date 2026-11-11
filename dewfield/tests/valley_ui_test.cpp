// SPDX-License-Identifier: MIT
#define main existing_ui_test_main
#include "ui_test.cpp"
#undef main
#include <string>
static void capture(const char*dir,const char*name) {
 if(!dir)return;std::string path=std::string(dir)+"/"+name+".ppm";FILE*f=std::fopen(path.c_str(),"wb");assert(f);
 std::fprintf(f,"P6\n%d %d\n255\n",WIDTH,HEIGHT);
 for(int i=0;i<WIDTH*HEIGHT;++i){auto c=pocket::framebuffer()[i];uint8_t b[]={uint8_t((c>>11)*255/31),uint8_t(((c>>5)&63)*255/63),uint8_t((c&31)*255/31)};std::fwrite(b,1,3,f);}std::fclose(f);
}
int main(int argc,char**argv) {
 const char*dir=argc>1?argv[1]:nullptr;
 game::init();game::key(pocket::Key::A);assert(page==Page::Character&&!state.character_chosen);
 game::key(pocket::Key::RIGHT);assert(state.appearance==1);game::key(pocket::Key::A);assert(page==Page::Intro&&state.character_chosen);
 intro_elapsed=500;intro_skip=true;intro_next();assert(page==Page::Farm);
 game::settings();setting_action(6);game::key(pocket::Key::LEFT);assert(!state.appearance);game::key(pocket::Key::B);
 assert(page==Page::Settings&&state.appearance==1);setting_action(6);game::key(pocket::Key::LEFT);game::key(pocket::Key::A);assert(page==Page::Settings&&!state.appearance);
 for(auto size:{std::pair<int,int>{240,240},{427,240},{240,427},{320,240},{480,320}})for(bool en:{false,true}) {
  WIDTH=size.first;HEIGHT=size.second;pocket::set_english(en);title_screen=false;
  for(auto p:{Page::Character,Page::Settings,Page::World,Page::Shop,Page::Fishing,Page::Intro}) {
   page=p;
   for(int frame=0;frame<(p==Page::Intro?10:1);++frame) {
    intro_scene=frame%5;if(p==Page::Intro)state.appearance=frame/5;intro_elapsed=2000;fishing.junk=true;fishing.phase=FishingSession::Phase::Caught;
    std::fill(pixels,pixels+WIDTH*HEIGHT+32,uint16_t(0xabcd));game::draw();
    for(int i=0;i<16;++i)assert(pixels[i]==0xabcd&&pixels[16+WIDTH*HEIGHT+i]==0xabcd);
    if(!en&&WIDTH==427){char name[64];std::snprintf(name,sizeof name,"page-%d-%d",int(p),frame);capture(dir,name);}
   }
  }
 }
 WIDTH=427;HEIGHT=240;pocket::set_english(false);page=Page::Farm;
 for(int region=0;region<RegionCount;++region) {
  farm::fresh(state);state.character_chosen=true;state.appearance=1;state.story=5;state.minute=720;state.region=region;state.visited|=1u<<region;
  if(region==unsigned(Region::Mine))farm::enter_mine(state);else if(indoors(region)){state.px=16*16+8;state.py=17*16+8;}
  else if(region==unsigned(Region::Town)){state.px=15*16+8;state.py=16*16+8;}else if(region==unsigned(Region::Lake)){state.px=25*16+8;state.py=11*16+8;}
  else {state.px=14*16+8;state.py=14*16+8;}
  base_dirty=true;cached_geometry=-1;draw_base();game::draw();char name[64];std::snprintf(name,sizeof name,"region-%d",region);capture(dir,name);
 }
 farm::fresh(state);state.energy=100;page=Page::Fishing;fishing.phase=FishingSession::Phase::Caught;fishing.junk=true;
 auto retry=layout().confirm(174,25);tap(retry.x+8,retry.center_y());assert(page==Page::Fishing&&state.bait==11);
 puts("PASS: farmer selection, cancel/confirm, tool-compatible appearance, opening story, new maps and guarded bilingual layouts");
}
