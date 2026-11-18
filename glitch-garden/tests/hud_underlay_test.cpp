// SPDX-License-Identifier: MIT
#include "../hud_underlay.hpp"
#include <cassert>
#include <iostream>
int main(){using namespace garden;HudUnderlay cache;cache.configure(64,48,10,39);std::vector<uint16_t> image(64*48);for(unsigned i=0;i<image.size();++i)image[i]=i;auto original=image;cache.capture(image.data());
 std::fill(image.begin(),image.end(),65535);cache.restore(image.data());for(int y=0;y<48;++y)for(int x=0;x<64;++x)assert(image[y*64+x]==(y<10||y>=39?original[y*64+x]:65535));
 cache.configure(32,20,5,13);assert(cache.pixels.size()==32*12);std::cout<<"HUD restoration preserves the world and handles geometry changes\n";
}
