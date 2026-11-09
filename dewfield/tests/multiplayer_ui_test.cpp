// SPDX-License-Identifier: MIT
#define main existing_ui_test_main
#include "ui_test.cpp"
#undef main
int main() {
  game::init();
  auto &room=dewfield_net::room;
  for(auto size:{std::pair<int,int>{240,240},{240,427},{427,240},{320,240},{480,320}}){
    WIDTH=size.first;HEIGHT=size.second;
    for(bool en:{false,true})for(bool web:{false,true}){
      pocket::set_english(en);room.browser=web;title_screen=true;
      game::key(pocket::Key::Y);assert(page==Page::Multiplayer&&!title_screen);
      for(bool edit:{false,true})for(bool port:{false,true}){
        multiplayer_edit=edit;multiplayer_port=port;
        for(auto mode:{dewfield_net::Mode::Offline,dewfield_net::Mode::Joining,dewfield_net::Mode::Hosting,dewfield_net::Mode::Guest}){
          room.mode=mode;room.ready=mode==dewfield_net::Mode::Hosting||mode==dewfield_net::Mode::Guest;
          for(auto&f:room.farmers)f.pose.active=room.ready;
          std::fill(pixels,pixels+WIDTH*HEIGHT+32,uint16_t(0xabcd));game::draw();
          for(int i=0;i<16;++i)assert(pixels[i]==0xabcd&&pixels[16+WIDTH*HEIGHT+i]==0xabcd);
        }
      }
      room.leave();multiplayer_edit=false;
      multiplayer_action(0);assert(multiplayer_edit);
      game::key(pocket::Key::X);assert(multiplayer_ip[0]==0);
      multiplayer_digit_press(0);multiplayer_digit_press(8);multiplayer_digit_press(1);
      assert(!std::strcmp(multiplayer_ip,"192"));
      multiplayer_digit_press(11);assert(!std::strcmp(multiplayer_ip,"19"));
      game::key(pocket::Key::Y);assert(!multiplayer_edit);
      game::key(pocket::Key::B);assert(title_screen);
    }
  }
  // A visitor never serializes the host farm; leaving restores the backup.
  WIDTH=HEIGHT=240;game::init();state.day=18;state.gold=449;
  room.original=state;room.restore=true;room.mode=dewfield_net::Mode::Guest;room.ready=true;
  state.day=2;state.gold=90;uint8_t bytes[farm::SaveBytes];
  assert(game::serialize(bytes,sizeof bytes)==0);room.leave();
  assert(state.day==18&&state.gold==449);
  // The real delayed tool commit must submit an authoritative operation.
  // Previewing an animation must neither mutate the plot nor enqueue a command.
  farm::fresh(state);room.mode=dewfield_net::Mode::Guest;room.ready=true;room.my_id=1;
  room.farmers[1].pose.active=true;farm::equip_item(state,1);
  page=Page::Farm;title_screen=false;
  int plot=farm::plot_index(14,14);assert(!state.plots[plot].tilled);
  action(14,14);assert(swing&&room.count==0&&!state.plots[plot].tilled);
  for(int i=0;i<40;++i)game::tick(33);
  assert(room.count==1&&state.plots[plot].tilled);
  assert(room.pending[room.first].command.op==dewfield_net::Op::Act);
  room.leave();
  std::puts("PASS: room and keypad input, browser UI, guarded multilingual layouts, guest save isolation");
}
