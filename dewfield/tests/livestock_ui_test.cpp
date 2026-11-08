// SPDX-License-Identifier: MIT
#define main old_ui_test_main
#include "ui_test.cpp"
#undef main
#include <string>
static void capture(const std::string &path) {
 FILE*f=std::fopen(path.c_str(),"wb");assert(f);std::fprintf(f,"P6\n%d %d\n255\n",WIDTH,HEIGHT);
 for(int i=0;i<WIDTH*HEIGHT;++i){auto c=framebuffer()[i];unsigned char rgb[]={uint8_t((c>>11)*255/31),uint8_t(((c>>5)&63)*255/63),uint8_t((c&31)*255/31)};std::fwrite(rgb,1,3,f);}std::fclose(f);
}
int main(int argc,char **argv) {
 WIDTH=HEIGHT=240;game::init();title_screen=false;page=Page::Farm;
 std::memset(state.cleared,1,sizeof(state.cleared));state.auto_run=false;state.energy=0;
 state.px=10*16+8;state.py=14*16+8;stop_motion();walk_distance=0;swing=0;held_keys[1]=true;
 for(int ms=0;ms<1000;++ms){game::tick(1);assert(walking);assert(walk_distance<=32);}
 assert(walk_distance>=31);held_keys[1]=false;stop_motion();game::tick(1);assert(!walking);
 // Opening pauses game time, persists between scenes, and replay grants nothing.
 game::init();title_screen=true;page=Page::Farm;game::key(Key::A);assert(page==Page::Intro&&!title_screen);
 auto minute=state.minute;game::tick(200);assert(state.minute==minute);game::key(Key::A);assert(intro_scene==1&&state.intro==1);
 uint8_t save[farm::SaveBytes];auto saved_size=game::serialize(save,sizeof(save));game::load(save,saved_size);assert(state.intro==1);
 for(int i=1;i<IntroScenes;++i){game::tick(200);game::key(Key::A);}
 assert(page==Page::Farm&&state.intro==IntroScenes&&state.ranch&&animal_count(state)==2&&state.hay==20);
 start_intro(true);game::tick(200);game::key(Key::B);game::key(Key::A);
 assert(page==Page::Story&&animal_count(state)==2&&state.hay==20);
 state.px=RanchX*16+8;state.py=(RanchY+1)*16+8;state.gold=9999;open_ranch();assert(page==Page::Ranch);
 ranch_action(2);assert(state.trough==4);ranch_action(0);assert(page==Page::Supply);
 supply_kind=5;supply_male=false;supply_action(1);assert(state.animals[2].kind==5);
 ranch_index=2;state.animals[2].age=5;open_page(Page::Animal);
 state.px=11*16+8;state.py=18*16+8;
 animal_action(3);assert(page==Page::Farm&&route&&butcher_pending==2);
 for(int i=0;i<200&&page==Page::Farm;++i)game::tick(33);
 assert(page==Page::Butcher&&butcher_pending==-1&&state.animals[2].kind==5);
 butcher_action(false);assert(page==Page::Animal&&state.animals[2].kind==5);
 open_page(Page::Butcher);selected_row=0;ranch_key(Key::A);assert(state.animals[2].kind==5);
 open_page(Page::Butcher);selected_row=1;ranch_key(Key::A);assert(!state.animals[2].kind&&item_count(state,34)==4);
 // The shed's side and rear are solid; interaction routes to its front.
 for(int y=RanchY-2;y<=RanchY;++y)for(int x=RanchX-1;x<=RanchX+1;++x)assert(!walkable(state,x,y));
 go_to(Region::Farm,RanchX,RanchY,true);
 for(int i=0;i<300&&page==Page::Farm;++i)game::tick(33);
 assert(page==Page::Ranch&&state.px/16==RanchX&&state.py/16==RanchY+1);
 // The extra bag page preserves selection and allows cross-page dragging.
 assert(swap_slots(state,4,41));select_slot(state,41);open_page(Page::Bag);
 assert(bag_page==1&&bag_selection==41);game::key(Key::L);assert(bag_page==0);
 auto first=layout().bag_slot(0),second=layout().bag_slot(28),m=layout().modal();
 game::pointer(Phase::DOWN,first.center_x(),first.center_y());
 game::pointer(Phase::MOVE,m.x+180,m.y+15);assert(bag_page==1&&drag_active);
 game::pointer(Phase::MOVE,second.center_x(),second.center_y());game::pointer(Phase::UP,second.center_x(),second.center_y());
 assert(state.slots[28]==1&&state.active_slot==41);select_slot(state,28);
 saved_size=game::serialize(save,sizeof(save));game::load(save,saved_size);assert(state.active_slot==28&&state.slots[28]==1);
 // Story delivery uses the real dialogue action, and six rows remain visible.
 fresh(state);begin_ranch(state);state.story=5;state.ranch_chapter=4;talking_npc=1;
 page=Page::Dialogue;dialogue_action(0);assert(state.ranch_chapter==4&&std::strstr(letter,"6"));
 assert(change_item(state,17,6)&&change_item(state,40,4));sync_inventory(state);
 dialogue_action(0);assert(state.ranch_chapter==5&&std::strstr(letter,ranch_completion(5,english())));
 talking_npc=3;assert(change_item(state,42,1)&&change_item(state,33,1)&&change_item(state,31,2));sync_inventory(state);
 dialogue_action(0);assert(state.ranch_chapter==5);state.mine_rewards|=1;dialogue_action(0);assert(state.ranch_chapter==6);
 for(bool en:{false,true})for(int chapter=1;chapter<=RanchStoryChapters;++chapter) {
  const char*value=ranch_completion(chapter,en);char rows[12][128]{};assert(wrapped(value,rows,12,200)<=6);
 }
 // A sixth line really paints; the old paragraph helper silently stopped at five.
 pocket::set_english(true);std::fill(pixels,pixels+WIDTH*HEIGHT+32,0x1ace);
 paragraph(10,10,"A\nB\nC\nD\nE\nF",200,6,Text);
 bool sixth=false;for(int y=85;y<97;++y)for(int x=10;x<20;++x)sixth|=framebuffer()[y*WIDTH+x]!=0x1ace;assert(sixth);
 // Every new page and scene on portrait, square and landscape layouts, both languages.
 for(auto dims:{std::pair<int,int>{240,240},{400,240},{240,427}})for(bool en:{false,true}) {
  WIDTH=dims.first;HEIGHT=dims.second;pocket::set_english(en);
  for(Page p:{Page::Bag,Page::Ranch,Page::Supply,Page::Incubate,Page::Animal,Page::Butcher}) {
   ranch_index=0;page=p;std::fill(pixels,pixels+WIDTH*HEIGHT+32,0x1ace);game::draw();
   for(int i=0;i<16;++i)assert(pixels[i]==0x1ace&&pixels[16+WIDTH*HEIGHT+i]==0x1ace);
   if(argc>1&&WIDTH==240&&HEIGHT==240&&!en)capture(std::string(argv[1])+"/page-"+std::to_string(int(p))+".ppm");
  }
  page=Page::Dialogue;talking_npc=3;
  for(int chapter:{4,5,6}) {
   state.ranch_chapter=chapter;
   std::snprintf(letter,sizeof(letter),"%s",chapter==6?ranch_completion(chapter,en):ranch_prompt(state,en));
   char rows[12][128]{};assert(wrapped(letter,rows,12,200)<=6);
   std::fill(pixels,pixels+WIDTH*HEIGHT+32,0x1ace);game::draw();
   for(int j=0;j<16;++j)assert(pixels[j]==0x1ace&&pixels[16+WIDTH*HEIGHT+j]==0x1ace);
   if(argc>1&&WIDTH==240&&HEIGHT==240&&!en)capture(std::string(argv[1])+"/ranch-story-"+std::to_string(chapter)+".ppm");
  }
  page=Page::Intro;
  for(int i=0;i<IntroScenes;++i) {
   intro_scene=i;intro_elapsed=1800;intro_skip=false;
   std::fill(pixels,pixels+WIDTH*HEIGHT+32,0x1ace);game::draw();
   for(int j=0;j<16;++j)assert(pixels[j]==0x1ace&&pixels[16+WIDTH*HEIGHT+j]==0x1ace);
   if(argc>1&&WIDTH==240&&HEIGHT==240&&!en)capture(std::string(argv[1])+"/intro-"+std::to_string(i)+".ppm");
  }
 }
 puts("PASS: fractional slow walking never flashes idle; resumable/skippable prologue; replay safety; ranch supplies, feeding, two-step pork confirmation and guarded bilingual layouts");
}
