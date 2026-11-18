// SPDX-License-Identifier: MIT
#include "../hud.hpp"
#include "../hud_underlay.hpp"
#include <cassert>
#include <iostream>
using namespace garden;
int main(){FilteredArt art;art.bytes.resize(garden_art::pixel_bytes);assert(unpack(garden_art::packed_pixels,art.bytes));art.cache_budget=1536*1024;
 for(int native_width:{296,480,854}){int height=native_width==296?240:480;float scale=std::min(float(native_width)/296,float(height)/240);Layout layout;layout.w=std::lround(native_width/scale);layout.h=std::lround(height/scale);layout.bx=38;layout.by=93;layout.cell=25;
  Game game;game.start(0);std::vector<uint16_t> lawn(size_t(native_width)*height),cached,expected;for(unsigned i=0;i<lawn.size();++i)lawn[i]=uint16_t(i*17);cached=lawn;
  Paint base{cached.data(),native_width,height,native_width,art,scale};draw_hud_base(base,game,layout);HudUnderlay underlay;underlay.configure(native_width,height,base.pixel(56),base.pixel(layout.h-38));underlay.capture(cached.data());
  HudPatch currency,progress;auto c=base.physical({6,25,layout.w<260?28:36,17}),w=base.physical(layout.wave());currency.capture(cached.data(),native_width,c.x,c.y,c.w,c.h);progress.capture(cached.data(),native_width,w.x,w.y,w.w,w.h);
  for(int frame=0;frame<48;++frame){game.stars=(frame*13)%150;game.elapsed=frame*1.37f;int selection=frame%(plant_count+1);layout.seed_offset=frame%std::max(1,plant_count-layout.seed_visible()+1);
   underlay.restore(cached.data());draw_hud_cards(base,game,layout,selection);art.release_hud_cache();currency.restore(cached.data(),native_width);progress.restore(cached.data(),native_width);draw_hud_dynamic(base,game,layout);
   expected=lawn;Paint full{expected.data(),native_width,height,native_width,art,scale};draw_hud_base(full,game,layout);draw_hud_cards(full,game,layout,selection);draw_hud_dynamic(full,game,layout);
   assert(cached==expected);
  }
 }
 std::cout<<"HUD reuse matches clean rendering for selection, disabled cards, arrows, shovel, currency and progress across three viewports\n";
}
