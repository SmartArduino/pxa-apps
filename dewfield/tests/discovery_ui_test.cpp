// SPDX-License-Identifier: MIT
#define main existing_ui_test_main
#include "ui_test.cpp"
#undef main
int main() {
  game::init();title_screen=false;WIDTH=HEIGHT=240;
  open_page(Page::Guide);game::key(pocket::Key::Y);assert(page==Page::Collections);
  auto m=layout().modal();auto tab=layout().collection_tab(3);
  panel_tap(tab.center_x(),tab.center_y());assert(collection_category==3);
  unsigned before=state.gold;game::key(pocket::Key::A);assert(state.gold==before);
  state.harvested=10;game::key(pocket::Key::A);assert(state.gold==before+80);
  game::key(pocket::Key::A);assert(state.gold==before+80);
  game::key(pocket::Key::B);assert(page==Page::Guide);
  auto finds=layout().journal_tab(3);panel_tap(finds.center_x(),finds.center_y());assert(page==Page::Collections);
  game::key(pocket::Key::R);assert(collection_category==1);
  game::key(pocket::Key::DOWN);assert(selected_row==2);
  assert(game::back()&&page==Page::Guide);
  page=Page::Farm;state.px=23*16+8;state.py=15*16+8;farm::select_slot(state,6);dir=1;swing=0;
  int energy=state.energy,old_saves=saves;facing_action();
  assert(page==Page::Fishing&&state.energy==energy-4&&saves>old_saves);
  facing_action();assert(fishing.phase==farm::FishingSession::Phase::Waiting);
  while(fishing.phase==farm::FishingSession::Phase::Waiting)game::tick(33);
  game::key(pocket::Key::A);assert(fishing.phase==farm::FishingSession::Phase::Reeling);
  while(std::abs(fishing.cursor()-fishing.center)>55)game::tick(16);
  auto reel=layout().confirm(174,25);panel_tap(reel.center_x(),reel.center_y());
  assert(page==Page::Fishing&&fishing.phase==farm::FishingSession::Phase::Caught&&state.fish==2);
  int fish_id=fishing.species;game::key(pocket::Key::X);
  assert(page==Page::Collections&&collection_category==2&&selected_row==fish_id);
  game::key(pocket::Key::B);assert(page==Page::Fishing);
  game::key(pocket::Key::A);assert(fishing.phase==farm::FishingSession::Phase::Waiting&&state.energy==energy-8&&state.fish==2);
  game::key(pocket::Key::B);assert(page==Page::Farm);
  // Rendering all categories, selections and fishing phases fits each guarded viewport in both languages.
  farm::change_item(state,12,2);farm::change_item(state,22,1);
  for(auto size:{std::pair<int,int>{240,240},{400,240},{412,412},{240,427}}) {
    WIDTH=size.first;HEIGHT=size.second;base_dirty=true;
    for(bool en:{false,true}) {
      pocket::set_english(en);
      for(int category=0;category<4;++category)for(int row=0;row<(category==3?6:collection_size(category));++row) {
        page=Page::Collections;collection_category=category;selected_row=row;
        std::fill(pixels,pixels+WIDTH*HEIGHT+32,uint16_t(0xabcd));game::draw();
        for(int i=0;i<16;++i)assert(pixels[i]==0xabcd&&pixels[16+WIDTH*HEIGHT+i]==0xabcd);
      }
      for(auto phase:{farm::FishingSession::Phase::Waiting,farm::FishingSession::Phase::Bite,farm::FishingSession::Phase::Reeling,farm::FishingSession::Phase::Caught,farm::FishingSession::Phase::Escaped}) {
        page=Page::Fishing;fishing.phase=phase;fishing.species=7;fishing.discovered=true;fishing.perfect=true;fishing.length=72;fishing.count=2;
        std::fill(pixels,pixels+WIDTH*HEIGHT+32,uint16_t(0xabcd));game::draw();
        if(phase==farm::FishingSession::Phase::Caught){fishing.count=1;game::draw();}
        for(int i=0;i<16;++i)assert(pixels[i]==0xabcd&&pixels[16+WIDTH*HEIGHT+i]==0xabcd);
      }
    }
  }
  puts("PASS: real journal/touch/controller collection, one-time rewards, persisted casting cost, catching/recasting/cancel input and bilingual guarded fishing/catalog views");
}
