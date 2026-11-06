// SPDX-License-Identifier: MIT
#include "../art.hpp"
#include "../scene_blit.hpp"
#include <cassert>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <vector>

int main(int argc, char **argv) {
  constexpr int width=48, height=40;
  std::vector<uint16_t> expected(width*height), actual(width*height);
  for(int kind=0;kind<art::CreatureCount;++kind)
    for(int frame=0;frame<art::CreatureFrames;++frame) {
      const auto *indices=art::creature_pixels[kind*art::CreatureFrames+frame];
      uint16_t original[24*24];
      for(int i=0;i<24*24;++i) {
        assert(indices[i]<art::CreaturePaletteSize);
        original[i]=art::creature_palettes[kind][indices[i]];
        assert(!indices[i] || original[i]);
      }
      for(int light=0;light<=255;++light) {
        uint16_t palette[art::CreaturePaletteSize];
        for(int i=0;i<art::CreaturePaletteSize;++i)
          palette[i]=starsoil_color::dim(art::creature_palettes[kind][i],unsigned(light));
        for(bool flip:{false,true})
          for(auto pos:{std::pair{-25,-25},std::pair{-7,-3},std::pair{0,0},
                        std::pair{12,8},std::pair{40,32},std::pair{48,40}}) {
            std::fill(actual.begin(),actual.end(),0x1357);
            expected=actual;
            starsoil_scene::blit(expected.data(),width,height,pos.first,pos.second,
                                 original,24,24,flip,light,5,37);
            starsoil_scene::blit_indexed(actual.data(),width,height,pos.first,pos.second,
                                         indices,24,24,palette,flip,5,37);
            assert(actual==expected);
          }
      }
    }
  puts("PASS all 54 creature frames, 256 light levels, both directions, clipping and opaque black match RGB565 drawing exactly");
  const size_t bytes=sizeof(art::creature_pixels)+sizeof(art::creature_palettes);
  printf("Creature graphics %zu bytes, previous five-creature RGB565 set %d bytes\n",bytes,5*6*24*24*2);
  if(argc<2 || std::strcmp(argv[1],"--benchmark"))return 0;
  uint16_t originals[art::CreatureCount*art::CreatureFrames][24*24];
  for(int f=0;f<art::CreatureCount*art::CreatureFrames;++f)
    for(int i=0;i<24*24;++i)originals[f][i]=art::creature_palettes[f/6][art::creature_pixels[f][i]];
  for(int pass=0;pass<4;++pass) {
    const bool indexed=pass%2;
    unsigned checksum=0;
    auto start=std::chrono::steady_clock::now();
    for(int i=0;i<200000;++i) {
      int f=i%(art::CreatureCount*art::CreatureFrames),light=28+i%228;
      if(indexed) {
        uint16_t palette[art::CreaturePaletteSize];
        for(int c=0;c<art::CreaturePaletteSize;++c)
          palette[c]=starsoil_color::dim(art::creature_palettes[f/6][c],unsigned(light));
        starsoil_scene::blit_indexed(actual.data(),width,height,12,8,art::creature_pixels[f],
                                     24,24,palette,i%2,0,height);
      } else {
        starsoil_scene::blit(actual.data(),width,height,12,8,originals[f],24,24,i%2,light,0,height);
      }
      checksum+=actual[20*width+20];
    }
    auto us=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-start).count();
    printf("%s 200000 draws %lld us checksum %u\n",indexed?"indexed":"RGB565",(long long)us,checksum);
  }
}
