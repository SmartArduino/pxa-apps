// SPDX-License-Identifier: MIT
#define main previous_ui_test_main
#include "ui_test.cpp"
#undef main
static void where(farm::Region region,int x,int y){state.region=unsigned(region);state.visited|=1u<<state.region;state.px=x*16+8;state.py=y*16+8;page=Page::Farm;base_dirty=true;swing=0;toast_ms=0;}
int main(){
 game::init();title_screen=false;WIDTH=HEIGHT=240;state.minute=600;
 where(farm::Region::Home,15,8);action(15,7);assert(page==Page::Forecast);
 where(farm::Region::Home,13,6);action(13,5);assert(page==Page::Calendar);
 auto m=layout().modal();panel_tap(m.x+14+6*29+5,m.y+60);assert(selected_row==6);
 game::key(pocket::Key::DOWN);assert(selected_row==13);
 // Real picking uses the drawn TV/calendar surface rather than a hidden tile.
 where(farm::Region::Home,15,8);camera();game::draw();int tx=-1,ty=-1;
 assert(world_objects.pick(15*16+8,7*16, state,tx,ty)&&tx==15&&ty==7);
 int xx=15*16+8-camx,yy=7*16+8-camy;
 game::pointer(pocket::Phase::DOWN,xx,yy);game::pointer(pocket::Phase::UP,xx,yy);
 for(int i=0;i<500&&page==Page::Farm;++i)game::tick(33);
 assert(page==Page::Forecast);
 where(farm::Region::Town,7,8);action(7,7);assert(state.region==unsigned(farm::Region::SeedShop));
 where(farm::Region::SeedShop,15,11);action(15,10);assert(page==Page::Shop);
 state.minute=1020;page=Page::Farm;action(15,10);assert(page==Page::Farm);state.minute=600;
 where(farm::Region::Farm,8,10);state.produce[0]=2;state.produce[2]=4;state.energy=30;farm::equip_item(state,12);dir=0;
 facing_action();assert(state.produce[0]==1&&state.produce[2]==4&&state.energy==30&&deposit_ms);
 assert(deposited_item==12);swing=0;deposit_ms=0;farm::select_slot(state,-1);facing_action();assert(page==Page::Shipping);
 panel_tap(m.x+100,m.y+182);assert(state.produce[0]==2&&state.shipping==0);
 where(farm::Region::Farm,14,14);farm::equip_item(state,12);facing_action();assert(state.energy==40&&state.produce[0]==1);
 // Empty final seed cannot leave a phantom bag or reuse the selected slot.
 state.seeds[0]=1;farm::equip_item(state,8);where(farm::Region::Farm,13,15);state.plots[farm::plot_index(13,14)]={1,0,0,0};dir=0;action(13,14);
 assert(!state.seeds[0]&&held_item()==0&&state.active_slot==255&&action_tool==1);
 int before_dir=dir;game::key(pocket::Key::RIGHT);assert(dir==before_dir);swing=0;
 for(auto size:{std::pair<int,int>{240,240},{296,240},{412,412},{240,427},{400,240}}){
  WIDTH=size.first;HEIGHT=size.second;
  for(bool en:{false,true}){pocket::set_english(en);
   for(auto screen:{Page::Calendar,Page::Forecast,Page::Shipping,Page::Shop,Page::World,Page::People,Page::Bag}){
    page=screen;selected_row=0;base_dirty=true;std::fill(pixels,pixels+WIDTH*HEIGHT+32,0xabcd);game::draw();
    for(int i=0;i<16;++i)assert(pixels[i]==0xabcd&&pixels[16+WIDTH*HEIGHT+i]==0xabcd);
   }
  }
 }
 puts("PASS: actual TV/calendar/counter input, shop hours, held-item shipping vs eating, seed depletion, tool direction lock and guarded bilingual displays");
}
