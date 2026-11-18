// SPDX-License-Identifier: MIT
#include "../filtered_art.hpp"
#include "../paint.hpp"
#include <cassert>
#include <iostream>
int main(){using namespace garden;
 assert(sample_axis(0,2,4).first==0);assert(sample_axis(3,2,4).second==1);
 auto red=premultiply(rgb(255,0,0),255),transparent=premultiply(rgb(0,255,0),0);
 auto edge=filtered_pixel(red,transparent,red,transparent,128,0);
 assert((edge>>24)==128);assert(((edge>>16)&255)==128);assert(((edge>>8)&255)==0);
 assert(over(transparent,rgb(0,0,255))==rgb(0,0,255));assert(over(red,rgb(0,0,255))==rgb(255,0,0));
 auto blended=over(edge,rgb(0,0,255));assert((blended>>11)>=15);assert((blended&31)>=15);
 FilteredArt art;art.bytes.resize(garden_art::pixel_bytes);assert(unpack(garden_art::packed_pixels,art.bytes));
 auto a=art.prepare(0,32,32,false);assert(a);assert(art.prepare(0,32,32,false)==a);assert(art.hits==1);
 for(unsigned i=0;i<150;++i){assert(art.prepare(i,90,90,false));assert(art.cache_bytes<=art.cache_budget);}
 std::vector<uint16_t> expected(32*32),restored(32*32);art.blit(expected.data(),32,32,32,0,{0,0,32,32});
 assert(art.prepare(garden_art::ui_pause_panel,360,350,false));
 art.blit(restored.data(),32,32,32,0,{0,0,32,32});assert(expected==restored);
 assert(art.arena.size()*4<=art.cache_budget);
 assert(!art.prepare(0,2048,2048,false));art.clear_cache();assert(art.cache_bytes==0);
 // Fractional translation spreads one pose's coverage continuously across the
 // display grid. Integer placement must remain byte-identical to normal blit.
 std::vector<uint16_t> normal(12),fractional(12);
 art.blit(normal.data(),12,1,12,0,{4,0,1,1});
 art.blit_subpixel(fractional.data(),12,1,12,0,{4,0,1,1},4.f);
 assert(normal==fractional);
 std::fill(fractional.begin(),fractional.end(),0);
 art.blit_subpixel(fractional.data(),12,1,12,0,{4,0,1,1},4.5f);
 assert(fractional[4]&&fractional[4]==fractional[5]);
 assert(!fractional[3]&&!fractional[6]);
 // Clipped fractional coverage must also be safe at either screen edge.
 art.blit_subpixel(fractional.data(),12,1,12,0,{0,0,2,1},-.5f);
 art.blit_subpixel(fractional.data(),12,1,12,0,{11,0,2,1},11.5f);
 Paint prewarm{nullptr,240,240,240,art,1,true,true};prewarm.arrow({0,0,20,29},true,true);assert(art.cache_bytes>0);
 std::cout<<"filter: sampling, premultiplied alpha, compositing, cache eviction passed\n";
}
