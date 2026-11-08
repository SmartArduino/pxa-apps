// SPDX-License-Identifier: MIT
#define main existing_ui_test_main
#include "ui_test.cpp"
#undef main
int main() {
  game::init();title_screen=false;WIDTH=HEIGHT=240;
  state.region=unsigned(farm::Region::Home);state.visited|=1u<<state.region;
  state.px=21*16+8;state.py=11*16+8;state.gathered[1]=2;state.produce[1]=1;
  action(21,10);assert(page==Page::Kitchen);
  auto modal=layout().modal();panel_tap(modal.x+180,modal.y+58);
  assert(state.meals[0]==1&&state.gathered[1]==0&&state.produce[1]==0);
  // Ingredient text and blank modal space are not action buttons.
  panel_tap(modal.x+70,modal.y+80);assert(state.meals[0]==1);
  state.energy=20;farm::equip_item(state,22);page=Page::Farm;facing_action();
  assert(page==Page::Eat&&state.energy==20&&state.meals[0]==1);game::key(Key::A);
  assert(state.energy==65&&state.meals[0]==0);
  state.region=unsigned(farm::Region::Town);state.visited|=1u<<state.region;
  state.px=18*16+8;state.py=11*16+8;
  auto order=farm::daily_order(state,0);farm::change_item(state,order.item,order.count);
  action(18,10);assert(page==Page::Orders);
  panel_tap(modal.x+180,modal.y+50);assert(state.orders_done==1);
  panel_tap(modal.x+180,modal.y+50);assert(state.orders_done==1);
  open_page(Page::Guide);selected_row=4;game::key(pocket::Key::DOWN);
  assert(selected_row==6);game::key(pocket::Key::A);assert(page==Page::Orders);
  open_page(Page::Guide);selected_row=5;game::key(pocket::Key::DOWN);
  assert(selected_row==7);game::key(pocket::Key::A);assert(page==Page::Kitchen);
  // Real navigation follows the woodland path, rather than teleporting.
  page=Page::Farm;state.story=3;go_to(farm::Region::Orchard,16,12,false);
  for(int i=0;i<5000&&(moving||route);++i)game::tick(33);
  assert(state.region==unsigned(farm::Region::Orchard)&&!moving&&!route);
  // A ground pickup is interactive even though its tile remains walkable;
  // direct pointer selection must gather it rather than merely walking on it.
  state.px=8*16+8;state.py=8*16+8;state.forage=0;state.day=29;
  page=Page::Farm;toast_ms=0;camera();
  int apples=state.gathered[0];game::draw();
  int xx=7*16+8-camx,yy=8*16+8-camy;
  game::pointer(pocket::Phase::DOWN,xx,yy);game::pointer(pocket::Phase::UP,xx,yy);
  for(int i=0;i<1000&&(moving||route);++i)game::tick(33);
  assert(state.gathered[0]==apples+2&&!route);
  state.gathered[0]=4;state.gathered[1]=4;state.gathered[2]=4;
  state.meals[0]=3;state.meals[1]=2;farm::sync_inventory(state);
  for(auto size:{std::pair<int,int>{240,240},{296,240},{412,412},{240,427},{400,240}}) {
    WIDTH=size.first;HEIGHT=size.second;
    for(bool en:{false,true}) {
      pocket::set_english(en);
      for(auto screen:{Page::Farm,Page::Guide,Page::World,Page::Orders,Page::Kitchen,Page::Bag}) {
        page=screen;base_dirty=true;
        std::fill(pixels,pixels+WIDTH*HEIGHT+32,uint16_t(0xabcd));game::draw();
        for(int i=0;i<16;++i)assert(pixels[i]==0xabcd&&pixels[16+WIDTH*HEIGHT+i]==0xabcd);
      }
    }
  }
  puts("PASS: actual stove/board clicks, ingredients, eating, input bounds, journal/controller access, orchard journey and multilingual guarded layouts");
}
