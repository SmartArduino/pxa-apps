// SPDX-License-Identifier: MIT
#define main original_ui_tests
#include "ui_test.cpp"
#undef main
#include "../pacing.hpp"
static int travel(bool run, int energy, int frame_ms, bool route_to_tile) {
  game::init(); title_screen=false; page=Page::Farm;
  farm::fresh(state); std::memset(state.cleared,1,sizeof(state.cleared));
  stop_motion(); clock_acc=0; swing=0;
  state.px=10*16+8; state.py=14*16+8;
  state.auto_run=run; state.energy=energy;
  held_keys[1]=!route_to_tile;
  if(route_to_tile) begin_route(22,14,false);
  int start=state.px;
  for(int elapsed=0;elapsed<1000;) {
    int duration=std::min(frame_ms,1000-elapsed);
    dewfield_pacing::advance(duration,[](uint32_t ms){game::tick(ms);});
    elapsed+=duration;
  }
  held_keys[1]=false;
  assert(state.energy==energy && state.py==14*16+8);
  return state.px-start;
}
static void resign(uint8_t *bytes) {
  uint32_t sum=2166136261u;
  for(int i=0;i<farm::SaveBytes-4;++i)sum=(sum^bytes[i])*16777619u;
  for(int i=0;i<4;++i)bytes[farm::SaveBytes-4+i]=sum>>(i*8);
}
int main() {
  for(bool route_to_tile:{false,true}) for(int frame:{16,33,100,200}) {
    int run=travel(true,100,frame,route_to_tile);
    int walk=travel(false,100,frame,route_to_tile);
    int tired=travel(true,0,frame,route_to_tile);
    assert(run>=108 && run<=112 && walk>=61 && walk<=64);
    assert(tired>=30 && tired<=32 && run>walk*16/10);
  }
  game::settings(); auto ui=layout();
  state.auto_run=true;
  tap(ui.setting(3).center_x(),ui.setting(3).center_y());
  assert(!state.auto_run);
  selected_row=3;game::key(Key::A);assert(state.auto_run);
  game::key(Key::DOWN);assert(selected_row==4);
  game::key(Key::DOWN);assert(selected_row==0);
  farm::State restored; uint8_t bytes[farm::SaveBytes];
  state.auto_run=false;assert(farm::encode(state,bytes,sizeof(bytes))==sizeof(bytes));
  assert(bytes[4]==9 && farm::decode(restored,bytes,sizeof(bytes)) && !restored.auto_run);
  bytes[559]=2;resign(bytes);assert(!farm::decode(restored,bytes,sizeof(bytes)));
  bytes[4]=5;resign(bytes);assert(farm::decode(restored,bytes,sizeof(bytes)) && restored.auto_run);
  for(auto size:{std::pair<int,int>{240,240},{296,240},{412,412},{240,427},{400,240}}) {
    WIDTH=size.first;HEIGHT=size.second;
    for(bool en:{false,true}) {
      pocket::set_english(en);page=Page::Settings;
      std::fill(pixels,pixels+WIDTH*HEIGHT+32,0xabcd);game::draw();
      for(int i=0;i<16;++i)assert(pixels[i]==0xabcd && pixels[16+WIDTH*HEIGHT+i]==0xabcd);
    }
  }
  puts("PASS: default running, touch and controller toggle, exact walk/run speed under 16..200 ms frames, no running energy cost, tired speed, five guarded layouts and v5/v6 settings saves");
}
