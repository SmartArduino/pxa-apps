// SPDX-License-Identifier: MIT
#include "../filtered_art.hpp"
#include <cassert>
#include <iostream>
int main(){using namespace garden;FilteredArt art;art.cache_budget=64*1024;art.bytes.resize(garden_art::pixel_bytes);assert(unpack(garden_art::packed_pixels,art.bytes));
 for(unsigned id=0;id<540;id+=7)for(unsigned q=0;q<4;++q)for(float origin:{-8.f,13.f,66.f}){
  std::vector<uint16_t> exact(80*70),cached(80*70);for(unsigned i=0;i<exact.size();++i)exact[i]=cached[i]=rgb(60+i%90,90+i%130,40+i%80);
  Rect r{int(origin),5,35,45};float left=origin+q*.25f;art.cached_motion=false;art.blit_subpixel(exact.data(),80,70,80,id,r,left);
  art.cached_motion=true;art.blit_subpixel(cached.data(),80,70,80,id,r,left);assert(exact==cached);assert(art.cache_bytes<=art.cache_budget);
 }
 for(auto& e:art.cache)assert(!e.pinned);
 std::cout<<"Quarter-pixel sampling matches full sampling exactly at each phase; clipping, eviction and compaction passed\n";
}
