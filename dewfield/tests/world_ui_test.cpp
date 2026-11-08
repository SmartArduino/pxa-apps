// SPDX-License-Identifier: MIT
#define main existing_ui_test_main
#include "ui_test.cpp"
#undef main
static void arrive(farm::Region region, int x, int y, bool use = false) {
  go_to(region, x, y, use);
  for (int i = 0; i < 5000 && (moving || route); ++i)
    game::tick(33);
  if(state.region!=unsigned(region)||moving||route)
    std::fprintf(stderr,"Journey target=%u current=%u pos=%u,%u target=%d,%d minute=%u page=%u route=%d moving=%d portal=%d front=%d notice=%s\n",unsigned(region),state.region,state.px,state.py,targetx,targety,state.minute,unsigned(page),route,moving,route_portal,approaching_portal,toast);
  assert(state.region == unsigned(region) && !moving && !route);
}
int main() {
  game::init();
  title_screen = false;
  WIDTH = HEIGHT = 240;
  state.story = 3;
  arrive(farm::Region::Town, 15, 12);
  arrive(farm::Region::Forest, 15, 12);
  arrive(farm::Region::Lake, 24, 12);
  arrive(farm::Region::Home, 16, 17);
  assert(state.visited == 31);
  // An NPC can be tracked across areas and talked to using the real controller.
  state.minute = 600;
  following_npc = 0;
  auto npc = farm::npc_pose(state, 0);
  arrive(npc.region, npc.x / 16, npc.y / 16, true);
  assert(page == Page::Dialogue && talking_npc == 0 && (state.met & 1));
  auto bond = state.bonds[0];
  page = Page::Farm;
  following_npc = 0;
  npc = farm::npc_pose(state, 0);
  arrive(npc.region, npc.x / 16, npc.y / 16, true);
  assert(state.bonds[0] == bond);
  state.produce[0] = 2;
  farm::equip_item(state, 12);
  dialogue_action(1);
  assert(state.produce[0] == 1);
  dialogue_action(1);
  assert(state.produce[0] == 1);
  // New screens at supported sizes, languages and map states stay in bounds.
  for (auto size :
       {std::pair<int, int>{240, 240}, {320, 240}, {240, 320}, {480, 320}}) {
    WIDTH = size.first;
    HEIGHT = size.second;
    for (int region = 0; region < farm::RegionCount; ++region) {
      state.region = region;
      if(region==int(farm::Region::Mine))farm::enter_mine(state,1);
      state.visited = 255;
      state.px = region == 4 ? 264 : 392;
      state.py = 200;
      base_dirty = true;
      for (bool en : {false, true}) {
        pocket::set_english(en);
        for (auto screen : {Page::Farm, Page::World, Page::People, Page::Story,
                            Page::Dialogue}) {
          page = screen;
          std::fill(pixels, pixels + WIDTH * HEIGHT + 32, uint16_t(0xabcd));
          game::draw();
          for (int i = 0; i < 16; ++i)
            assert(pixels[i] == 0xabcd &&
                   pixels[16 + WIDTH * HEIGHT + i] == 0xabcd);
        }
      }
    }
  }
  WIDTH = HEIGHT = 240;
  sleep_now();
  assert(state.region == 4 && page == Page::Morning &&
         farm::walkable(state, state.px / 16, state.py / 16));
  puts("PASS: actual region journeys, tracked NPC dialogue/gifts, guarded new "
       "pages, dawn at home");
}
