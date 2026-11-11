// SPDX-License-Identifier: MIT
#define main old_village_ui_main
#include "ui_test.cpp"
#undef main
#include <string>
static void scene(Region r,int x,int y) {fresh(state);state.intro=IntroScenes;state.minute=600;state.region=unsigned(r);state.visited|=1u<<state.region;state.px=x*16+8;state.py=y*16+8;title_screen=false;page=Page::Farm;base_dirty=true;cancel_tool();stop_motion();following_npc=following_animal=-1;toast_ms=0;}
static void capture(const std::string&path){FILE*f=std::fopen(path.c_str(),"wb");assert(f);std::fprintf(f,"P6\n%d %d\n255\n",WIDTH,HEIGHT);for(int i=0;i<WIDTH*HEIGHT;++i){auto c=framebuffer()[i];uint8_t v[]={uint8_t((c>>11)*255/31),uint8_t(((c>>5)&63)*255/63),uint8_t((c&31)*255/31)};std::fwrite(v,1,3,f);}std::fclose(f);}
int main(int argc,char**argv){
 WIDTH=HEIGHT=240;game::init();scene(Region::Town,20,9);state.energy=50;assert(change_item(state,29,4));equip_item(state,29);auto p=npc_pose(state,1);state.px=p.x;state.py=(p.y/16+1)*16+8;dir=0;
 game::key(Key::B);assert(page==Page::Dialogue&&talking_npc==1&&state.energy==50&&item_count(state,29)==4&&selected_row==1);
 game::key(Key::A);assert(item_count(state,29)==3&&state.energy==50&&state.gift_day[1]==state.day);
 game::key(Key::B);assert(page==Page::Farm);game::key(Key::A);assert(page==Page::Eat&&item_count(state,29)==3&&state.energy==50);game::key(Key::B);assert(item_count(state,29)==3);
 game::key(Key::A);game::key(Key::LEFT);game::key(Key::A);assert(page==Page::Farm&&item_count(state,29)==3); // Highlighted cancel really cancels.
 game::key(Key::A);game::key(Key::A);assert(page==Page::Farm&&item_count(state,29)==2&&state.energy==58);
 scene(Region::Farm,ShippingX,ShippingY+1);state.produce[0]=13;state.produce[2]=7;equip_item(state,12);dir=0;
 game::key(Key::B);assert(page==Page::Shipping&&state.produce[0]==13);game::key(Key::A);assert(!state.produce[0]&&state.produce[2]==7&&state.shipping==208&&state.last_shipped_count==13);
 game::key(Key::Y);assert(state.produce[0]==13&&!state.shipping);game::key(Key::X);assert(state.produce[0]==12&&state.last_shipped_count==1);game::key(Key::Y);assert(state.produce[0]==13);
 game::key(Key::R);assert(shipping_page==1);game::key(Key::R);assert(shipping_page==2);game::key(Key::B);assert(page==Page::Farm);
 // Click an opaque cow pixel, walk to that exact animal, and open care.
 scene(Region::Farm,12,15);begin_ranch(state);for(auto &a:state.animals)a={};state.animals[0]={3,0,8,70};std::memset(state.cleared,1,sizeof(state.cleared));equip_item(state,5);state.show_joystick=state.show_action=false;
 game::draw();auto a=animal_pose(state,0,clock_acc);int tx=-1,ty=-1,animal=-1;
 assert(world_objects.pick(a.x,a.y-7,state,tx,ty,&animal)&&animal==0);
 auto energy=state.energy;tap(a.x-camx,a.y-7-camy);assert(following_animal==0&&route_action);
 for(int i=0;i<400&&page==Page::Farm;++i)game::tick(33);
 assert(page==Page::Animal&&ranch_index==0&&following_animal==-1&&state.energy==energy);
 game::key(Key::B);page=Page::Farm;state.px=a.x;state.py=(a.y/16+1)*16+8;dir=0;game::key(Key::B);assert(page==Page::Animal&&state.energy==energy);
 state.animals[1]={2,0,5,65};page=Page::Farm;state.px=11*16+8;state.py=18*16+8;dir=1;game::key(Key::B);assert(page==Page::Animal&&ranch_index==1); // The duck in front wins over the cow behind.
 scene(Region::Town,18,11);action(18,10);assert(page==Page::Orders);game::key(Key::Y);assert(page==Page::Calendar);
 scene(Region::Town,15,12);state.story=5;for(auto &b:state.bonds)b=30;state.friend_chapter[0]=1;talking_npc=0;page=Page::Dialogue;auto gold=state.gold;dialogue_action(0);assert(state.friend_chapter[0]==2&&state.gold==gold+100);
 setting_action(5);assert(!state.sound_effects);uint8_t raw[SaveBytes];auto size=game::serialize(raw,sizeof(raw));game::load(raw,size);assert(!state.sound_effects&&state.friend_chapter[0]==2);
 for(bool en:{false,true})for(int npc=0;npc<4;++npc)for(int chapter:{1,2}){char rows[12][128]{};assert(wrapped(friend_story_line(npc,chapter,en),rows,12,200)<=6);}
 for(auto dims:{std::pair<int,int>{240,240},{400,240},{240,427}})for(bool en:{false,true}){
  WIDTH=dims.first;HEIGHT=dims.second;pocket::set_english(en);
  for(Page view:{Page::Shipping,Page::Eat,Page::People,Page::Orders,Page::Calendar,Page::Settings,Page::Dialogue,Page::Animal}){
   page=view;food_item=29;shipping_page=0;shipping_selection=8;ranch_index=0;state.animals[0]={3,0,8,70};talking_npc=0;
   std::snprintf(letter,sizeof(letter),"%s",friend_story_line(0,2,en));std::fill(pixels,pixels+WIDTH*HEIGHT+32,0x1ace);game::draw();
   for(int i=0;i<16;++i)assert(pixels[i]==0x1ace&&pixels[16+WIDTH*HEIGHT+i]==0x1ace);
   if(argc>1&&WIDTH==240&&HEIGHT==240&&!en)capture(std::string(argv[1])+"/village-page-"+std::to_string(int(view))+".ppm");
  }
 }
 puts("PASS: controller eat/gift separation and cancellation, stack/one/retrieve, actual cow taps and follow-up panel, calendar, friendship delivery, effects persistence and guarded bilingual layouts");
}
