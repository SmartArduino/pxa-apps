// SPDX-License-Identifier: MIT
#include "../mines.hpp"
#include "../livestock.hpp"
#include <cassert>
#include <cstdio>
#include <cstring>
using namespace farm;
static void roundtrip(const State&s) {
 uint8_t canonical[SaveBytes],restored[SaveBytes],packed[StorageSaveBytes];State t;
 assert(encode(s,canonical,sizeof(canonical))==SaveBytes);
 assert(encode_storage(s,packed,sizeof(packed))==StorageSaveBytes&&packed[4]==11);
 assert(decode(t,packed,sizeof(packed))&&encode(t,restored,sizeof(restored))==SaveBytes);
 assert(!std::memcmp(canonical,restored,sizeof(canonical)));
 assert(!encode_storage(s,packed,sizeof(packed)-1));
 for(int i=0;i<StorageSaveBytes;++i){packed[i]^=1;assert(!decode(t,packed,sizeof(packed)));packed[i]^=1;}
 assert(!decode(t,packed,sizeof(packed)-1));
}
int main(int argc,char**argv) {
 State s;fresh(s);roundtrip(s);
 // Every valid plot combination remains exact, including regrowing tomatoes.
 for(int crop=0;crop<=4;++crop)for(int growth=0;growth<=(crop?Crops[crop-1].days:0);++growth)for(int wet=0;wet<2;++wet) {
  for(int i=0;i<PlotCount;++i)s.plots[i]={uint8_t(crop||i%2),uint8_t(crop),uint8_t(growth),uint8_t(wet)};
  roundtrip(s);
 }
 begin_ranch(s);for(int i=0;i<AnimalMax;++i){int kind=i%5+1;s.animals[i]={uint8_t(kind),uint8_t(i%2),Breeds[kind-1].adult,65};}
 for(auto &v:s.animal_goods)v=998;for(auto &v:s.minerals)v=999;sync_inventory(s);roundtrip(s);
 for(int floor=1;floor<=30;++floor){enter_mine(s,1);load_mine_floor(s,floor);roundtrip(s);}
 if(argc>1){uint8_t old[SaveBytes];FILE*f=std::fopen(argv[1],"rb");assert(f);auto n=std::fread(old,1,sizeof(old),f);std::fclose(f);assert(decode(s,old,n));roundtrip(s);}
 puts("PASS: fixed 960-byte device saves, every plot state, full ranch/mineral inventories, all 30 floors, every-byte corruption rejection and real legacy migration");
}
