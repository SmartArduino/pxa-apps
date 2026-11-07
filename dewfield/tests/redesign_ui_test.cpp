// SPDX-License-Identifier: MIT
#define main previous_ui_suite
#include "ui_test.cpp"
#undef main
static void place(int x,int y) {
  game::init();title_screen=false;page=Page::Farm;state.minute=600;state.story=3;
  std::memset(state.cleared,1,sizeof(state.cleared));stop_motion();
  state.px=x*16+8;state.py=y*16+8;clock_acc=0;swing=0;
  std::fill(std::begin(held_keys),std::end(held_keys),false);
}
int main() {
  // Walking across a front door's tile sideways is not an entrance.
  place(5,7);held_keys[1]=true;
  for(int i=0;i<5;++i)game::tick(33);
  held_keys[1]=false;assert(state.region==0 && state.px/16==6);
  game::key(Key::UP);assert(state.region==unsigned(farm::Region::Home));
  {
    farm_scene::Queue q;q.viewport={0,0,512,384};
    q.add(farm_scene::Kind::House,64,48,109,0,true);farm::State farm;farm::fresh(farm);
    int tx=0,ty=0;assert(!q.pick(104,45,farm,tx,ty));
    bool door=false;
    for(int y=94;y<108;++y)for(int x=96;x<112;++x)
      if(q.pick(x,y,farm,tx,ty)){assert(tx==6&&ty==7);door=true;}
    assert(door);
  }
  // A deliberate tap walks to the door's front, then crosses its threshold.
  place(14,14);begin_route(6,7,true);
  for(int i=0;i<1000&&state.region==0;++i)game::tick(33);
  assert(state.region==unsigned(farm::Region::Home));
  // Directly select a crop outside the hotbar, without rearranging inventory.
  place(14,14);state.produce[0]=2;farm::sync_inventory(state);
  int slot=0;while(state.slots[slot]!=12)++slot;
  assert(slot>=7);auto original=state;open_page(Page::Bag);auto ui=layout();
  tap(ui.bag_slot(slot).center_x(),ui.bag_slot(slot).center_y());
  assert(held_item()==12 && state.active_slot==slot);
  assert(!std::memcmp(original.slots,state.slots,sizeof(state.slots)));
  assert(farm::swap_slots(state,slot,27) && held_item()==12 && state.active_slot==27);
  uint8_t bytes[farm::SaveBytes];farm::State restored;
  assert(farm::encode(state,bytes,sizeof(bytes))==sizeof(bytes) && bytes[4]==7);
  assert(farm::decode(restored,bytes,sizeof(bytes)) && restored.active_slot==27);
  open_page(Page::Bag);assert(bag_selection==27 && selected_row==-1);
  game::key(Key::LEFT);assert(bag_selection==26);game::key(Key::RIGHT);assert(bag_selection==27);
  game::key(Key::A);assert(state.active_slot==255);
  game::key(Key::A);assert(state.active_slot==27 && held_item()==12);
  state.energy=50;page=Page::Farm;facing_action();assert(state.energy==60 && state.produce[0]==1);
  action_hold=true;game::tick(100);action_hold=false;
  assert(state.energy==60 && state.produce[0]==1); // One food/shipment per press.
  state.px=8*16+8;state.py=10*16+8;dir=0;swing=0;facing_action();
  assert(state.produce[0]==0 && state.shipping==16 && state.active_slot==255);
  // The occlusion decision follows the character's body, not just its feet.
  state.px=48;state.py=40;camx=camy=0;farm::select_slot(state,-1);
  assert(hud_opacity(ui.calendar())==3);
  state.px=232;state.py=232;assert(hud_opacity(ui.calendar())==8);
  assert(hud_opacity(ui.action(),5)==5);
  // Complete HUD composition must preserve the world beneath it, including
  // clipped edges; an opaque HUD should avoid touching any backup pixels.
  for(int opacity:{0,3,5,8}) {
    std::fill(pixels,pixels+WIDTH*HEIGHT+32,0xabcd);
    begin_hud(-3,-2,18,15,opacity);rect(0,0,15,13,0xffff);end_hud();
    assert(framebuffer()[0]==dewfield_hud::Backdrop::blend(0xabcd,0xffff,opacity));
    for(int i=0;i<16;++i)assert(pixels[i]==0xabcd && pixels[16+WIDTH*HEIGHT+i]==0xabcd);
  }
  puts("PASS: directional door thresholds, deliberate routed entry, full-backpack carrying/eating/shipping, selection-following drag, v7 saves, controller grid and bounded HUD transparency");
}
