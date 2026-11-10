// SPDX-License-Identifier: MIT
#define main prior_ui_tests
#include "ui_test.cpp"
#undef main
static void touch_scene(int x=10,int y=18){
 WIDTH=HEIGHT=240;game::init();fresh(state);state.intro=IntroScenes;state.minute=600;
 title_screen=false;page=Page::Farm;state.show_joystick=state.show_action=false;
 std::memset(state.cleared,1,sizeof(state.cleared));stop_motion();cancel_tool();clock_acc=anim_acc=0;toast_ms=0;
 state.px=x*16+8;state.py=y*16+8;select_slot(state,-1);base_dirty=true;
 std::fill(std::begin(held_keys),std::end(held_keys),false);game::draw();
}
static void advance(int ms){for(int t=0;t<ms;t+=16){game::tick(std::min(16,ms-t));game::draw();}}
int main(){
 // DOWN responds without waiting for release. A quick tap retains its original
 // world destination even if rendering scrolls the camera before UP.
 touch_scene();int wx=15*16+8,wy=18*16+8,x=wx-camx,y=wy-camy,start=state.px;
 game::pointer(Phase::DOWN,x,y);assert(route&&world_touch.active&&!world_touch.following);
 advance(96);assert(state.px>start);game::pointer(Phase::UP,x,y);assert(route&&!world_touch.active&&targetx==15);
 advance(1500);assert(state.px==wx&&state.py==wy&&!route&&!moving);
 // A stationary finger keeps moving after the initial tile, including camera
 // scrolling; release stops at the current safe tile, with no extra route.
 touch_scene();x=WIDTH-8;y=state.py-camy;start=state.px;game::pointer(Phase::DOWN,x,y);advance(1600);
 assert(world_touch.following&&!route&&state.px>start+110);
 game::pointer(Phase::UP,x,y);assert(!world_touch.active&&!route);advance(250);int stopped=state.px;advance(500);assert(state.px==stopped&&!moving);
 // A finger between tile centers cannot make the farmer oscillate forever.
 touch_scene(3,18);x=state.px-camx+9;y=state.py-camy;game::pointer(Phase::DOWN,x,y);advance(500);stopped=state.px;advance(500);assert(state.px==stopped);game::pointer(Phase::UP,x,y);
 // Drag retargets promptly, without new taps and without triggering controls
 // when the captured finger crosses their screen areas.
 touch_scene();x=190;y=state.py-camy;game::pointer(Phase::DOWN,x,y);advance(48);game::pointer(Phase::MOVE,12,y);assert(world_touch.following);advance(800);assert(dir==3);
 game::pointer(Phase::MOVE,layout().header_button(0).center_x(),layout().header_button(0).center_y());game::pointer(Phase::UP,220,20);assert(page==Page::Farm);
 // Cancellation and page changes cannot leave a held walk or a delayed tap.
 touch_scene();game::pointer(Phase::DOWN,190,150);advance(80);game::pointer(Phase::CANCEL,190,150);assert(!route&&!world_touch.active);advance(250);stopped=state.px;advance(300);assert(state.px==stopped);
 touch_scene();game::pointer(Phase::DOWN,190,150);open_page(Page::Bag);assert(!world_touch.active);game::pointer(Phase::UP,190,150);assert(page==Page::Bag&&!route);
 // A tool target is approached on DOWN but not struck until a tap finishes.
 touch_scene(10,17);select_slot(state,0);game::draw();x=10*16+8-camx;y=16*16+8-camy;assert(plot_index(10,16)>=0);
 game::pointer(Phase::DOWN,x,y);advance(160);assert(!swing&&!state.plots[plot_index(10,16)].tilled);game::pointer(Phase::UP,x,y);advance(500);assert(state.plots[plot_index(10,16)].tilled);
 touch_scene(10,17);select_slot(state,0);game::draw();x=10*16+8-camx;y=16*16+8-camy;
 game::pointer(Phase::DOWN,x,y);advance(260);assert(world_touch.following&&!swing);game::pointer(Phase::UP,x,y);advance(250);assert(!state.plots[plot_index(10,16)].tilled);
 touch_scene(10,17);select_slot(state,0);game::draw();x=10*16+8-camx;y=16*16+8-camy;
 game::pointer(Phase::DOWN,x,y);game::pointer(Phase::UP,x+24,y);advance(500);assert(!state.plots[plot_index(10,16)].tilled&&!route&&!swing);
 // Pure touch food: select from the bag, close it, touch the character, confirm.
 touch_scene();state.produce[2]=3;state.energy=50;sync_inventory(state);open_page(Page::Bag);int slot=0;while(state.slots[slot]!=14)++slot;
 tap(layout().bag_slot(slot).center_x(),layout().bag_slot(slot).center_y());assert(held_item()==14);tap(layout().close().center_x(),layout().close().center_y());game::draw();
 x=state.px-camx;y=state.py-camy-15;tap(x,y);assert(page==Page::Eat&&state.produce[2]==3&&state.energy==50);
 auto m=layout().modal();tap(m.x+60,m.y+190);assert(page==Page::Farm&&state.produce[2]==3);game::draw();
 auto caption=layout().caption(caption_width());tap(caption.center_x(),caption.center_y());assert(page==Page::Eat);tap(m.x+165,m.y+190);assert(page==Page::Farm&&state.produce[2]==2&&state.energy>50);
 // Holding the character with food starts movement mode; it never eats.
 game::draw();x=state.px-camx;y=state.py-camy-15;game::pointer(Phase::DOWN,x,y);advance(260);game::pointer(Phase::UP,x,y);assert(page==Page::Farm&&state.produce[2]==2);
 // Food tap remains food next to a shipping box; touching the box stays shipping.
 touch_scene(8,10);state.produce[0]=2;sync_inventory(state);equip_item(state,12);dir=0;game::draw();tap(state.px-camx,state.py-camy-12);assert(page==Page::Eat);eat_action(false);
 // A UI press must never start a walk; hold-to-drag inventory keeps priority.
 touch_scene();auto header=layout().header_button(0);game::pointer(Phase::DOWN,header.center_x(),header.center_y());assert(!world_touch.active&&!route);game::pointer(Phase::UP,header.center_x(),header.center_y());assert(page==Page::Bag);
 for(auto dims:{std::pair<int,int>{240,240},{400,240},{240,427}})for(bool en:{false,true}){
  WIDTH=dims.first;HEIGHT=dims.second;set_english(en);state.produce[0]=2;equip_item(state,12);toast_ms=0;route=false;
  for(Page p:{Page::Farm,Page::Eat}){page=p;food_item=12;std::fill(pixels,pixels+WIDTH*HEIGHT+32,0xabcd);game::draw();for(int i=0;i<16;++i)assert(pixels[i]==0xabcd&&pixels[16+WIDTH*HEIGHT+i]==0xabcd);}
 }
 puts("PASS: immediate press, fixed tap destination, camera-relative held walking, drag steering, release/cancel, deferred actions, touch-only eating and guarded bilingual layouts");
}
