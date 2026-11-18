#include "../filtered_art.hpp"
#include <cassert>
#include <vector>
#include <iostream>
int main(){using namespace garden;FilteredArt art;art.bytes.resize(garden_art::pixel_bytes);assert(unpack(garden_art::packed_pixels,art.bytes));std::vector<uint16_t> first(60*60),second(60*60);art.blit(first.data(),60,60,60,0,{0,0,40,50});art.prepare(garden_art::ui_button,180,30,false);auto before=art.cache_bytes;art.release_hud_cache();assert(art.cache_bytes<before);art.blit(second.data(),60,60,60,0,{0,0,40,50});assert(first==second);std::cout<<"Releasing static UI textures preserves actor cache pixels\n";}
