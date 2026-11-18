// SPDX-License-Identifier: MIT
#include "../packed_art.hpp"
#include "../art.hpp"
#include "../art_data.hpp"
#include <cassert>
#include <vector>
#include <array>
#include <cstdio>
#include <cstring>
int main(){std::vector<uint8_t> bytes(garden_art::pixel_bytes);assert(garden::unpack(garden_art::packed_pixels,bytes));unsigned nonzero=0;for(auto c:bytes)nonzero+=c!=0;assert(nonzero>100000);
 for(unsigned id=0;id<garden_art::frames.size();++id)for(unsigned y=0;y<garden_art::frames[id].h;++y){auto row=garden_art::source_rows[garden_art::source_first[id]+y];assert(row.left+row.count<=garden_art::frames[id].w);assert(row.opaque_begin+row.opaque_count<=row.count);assert(row.offset+row.count*2-row.opaque_count<=bytes.size());}
 for(auto p:garden_art::prepared){bytes.resize(p.words*4);assert(garden::unpack(std::span(garden_art::packed_prepared).subspan(p.offset,p.size),bytes));std::vector<uint32_t> words(p.words);std::memcpy(words.data(),bytes.data(),bytes.size());for(unsigned y=0;y<p.height;++y){auto at=words[y];if(!at)continue;assert(at<p.words);auto header=words[at];assert((header>>16)+(header&65535)<=p.width);assert(at+1+(header&65535)<=p.words);}}
 for(auto b:garden_art::backgrounds){bytes.resize(b.width*b.height*2);assert(garden::unpack(std::span(garden_art::packed_backgrounds).subspan(b.offset,b.size),bytes));}
 std::array<uint8_t,4> output{};assert(!garden::unpack(std::array<uint8_t,1>{128},output));assert(!garden::unpack(std::array<uint8_t,2>{3,0},output));assert(!garden::unpack(std::array<uint8_t,3>{128,0,0},output));assert(!garden::unpack(std::array<uint8_t,3>{128,1,0},output));assert(garden::unpack(std::array<uint8_t,5>{0,9,128,1,0},std::span<uint8_t>(output).first(0))==false);
 std::array<uint8_t,12> expanded{};assert(garden::unpack(std::array<uint8_t,7>{2,'a','b','c',133,3,0},expanded));for(unsigned i=0;i<12;++i)assert(expanded[i]=="abc"[i%3]);assert(!garden::unpack(std::array<uint8_t,7>{2,'a','b','c',133,3,0},output));
 std::puts("embedded source, prepared UI, native maps, overlap copies and malformed streams passed");}
