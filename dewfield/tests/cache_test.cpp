// SPDX-License-Identifier: MIT
#define main original_ui_fixture
#include "ui_test.cpp"
#undef main
int main() {
  game::init();title_screen=false;state.minute=720;state.weather=0;state.story=3;
  std::memset(state.cleared,1,sizeof(state.cleared));base_dirty=true;scene();
  const std::vector<uint16_t> ground(unlit_base,unlit_base+WorldPixelsW*WorldPixelsH);
  int geometry=cached_geometry;
  for(int minute:{1080,1130,1260,1440,720}) {
    state.minute=minute;scene();
    assert(cached_geometry==geometry && !base_dirty);
    assert(std::equal(ground.begin(),ground.end(),unlit_base));
    auto light=ambient_light(state);
    for(unsigned i=0;i<ground.size();++i)assert(base[i]==apply_light(ground[i],light));
  }
  state.minute=1260;state.plots[11]={1,2,1,1};scene();
  const std::vector<uint16_t> partial(base,base+WorldPixelsW*WorldPixelsH);
  base_dirty=true;scene();assert(std::equal(partial.begin(),partial.end(),base));
  puts("PASS: cached ground survives daylight changes, exact RGB565 relighting, partial wet-soil refresh matches complete redraw and geometry cache is reused");
}
