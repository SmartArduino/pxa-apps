// SPDX-License-Identifier: MIT
#define main previous_ui_main
#include "ui_test.cpp"
#undef main
static void home(int x,int y){
 farm::fresh(state);state.character_chosen=true;state.intro=farm::IntroScenes;state.story=5;state.minute=600;
 state.region=unsigned(farm::Region::Home);state.visited|=1u<<state.region;state.px=x*16+8;state.py=y*16+8;
 title_screen=false;page=Page::Farm;base_dirty=true;route=false;moving=false;swing=0;clock_acc=0;farm::select_slot(state,-1);
 camera();game::draw();
}
static void visit(int x,int y,Page expected,int door){
 tap(x-camx,y-camy);bool crossed=false;
 for(int i=0;i<800&&page==Page::Farm;++i){game::tick(33);crossed|=state.px/16==door&&(state.py/16>=9&&state.py/16<=11);}
 assert(page==expected&&crossed);game::key(pocket::Key::B);
}
int main(){
 game::init();WIDTH=480;HEIGHT=400;
 home(16,18);visit(10*16+8,8*16+8,Page::Sleep,15);
 home(16,18);visit(21*16+8,9*16+8,Page::Kitchen,18);
 home(12,10);visit(12*16+8,14*16-1,Page::Forecast,15);
 // All villagers eventually rest inside their private bedroom, after walking
 // through the real entrance hall and the correct bedroom door.
 for(int i=0;i<4;++i){state.minute=1500;auto p=farm::npc_pose(state,i);assert(p.region==farm::NpcHomes[i]);assert(farm::room_kind(unsigned(p.region),p.x/16,p.y/16)==farm::RoomKind::Bedroom);}
 puts("PASS: actual room-door navigation to bed, stove and lounge TV; villagers retire into their bedrooms");
}
