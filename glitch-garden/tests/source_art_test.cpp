// SPDX-License-Identifier: MIT
#include "filtered_art.hpp"
#include <cassert>
#include <iostream>
int main(){garden::FilteredArt art;art.bytes.resize(garden_art::pixel_bytes);assert(garden::unpack(garden_art::packed_pixels,art.bytes));uint64_t hash=1469598103934665603ull;
 for(unsigned id=0;id<garden_art::frames.size();++id){auto f=garden_art::frames[id];for(int y=0;y<f.h;++y)for(int x=0;x<f.w;++x){hash^=art.texel(id,x,y,false);hash*=1099511628211ull;}}
 std::cout<<hash<<"\n";
}
