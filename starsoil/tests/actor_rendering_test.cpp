// SPDX-License-Identifier: MIT
#include "../actor_renderer.hpp"
#include <cassert>
#include <cstdio>
#include <cstring>
using namespace starsoil;
template <const auto &Images> void check_encoded_images() {
  for (size_t row = 0; row < std::size(Images); ++row) {
    auto image = starsoil_actor::image<Images>(row);
    std::array<uint8_t, art::HumanWidth * art::HumanHeight> restored{};
    int previous = -1;
    for (int i = 0; i < image.count; ++i) {
      uint16_t command = image.pixels[i];
      int x = command & 31, y = (command >> 5) & 31;
      assert(x < art::HumanWidth && y < art::HumanHeight);
      int position = y * art::HumanWidth + x;
      assert(position > previous && (command >> 10) > 0);
      restored[position] = uint8_t(command >> 10);
      previous = position;
    }
    assert(std::equal(restored.begin(), restored.end(), Images[row]));
  }
}
uint64_t run(int repeat) {
  uint64_t fingerprint = 1469598103934665603ull;
  uint8_t equipment[8]{};
  constexpr int held[] = {0, CopperPick, IronAxe, HunterBow, StarWand};
  for (int rep=0; rep<repeat; ++rep)
    for (int outfit=0;outfit<4;++outfit) {
      equipment[0]=outfit==1?StrawHat:outfit==2?IronHelmet:outfit==3?CrystalHelmet:0;
      equipment[1]=outfit==2?IronMail:outfit==3?CrystalMail:0;
      equipment[2]=outfit==2?IronGreaves:outfit==3?CrystalGreaves:0;
      equipment[3]=outfit%2?SageRobe:RedTunic;
      equipment[4]=TealCape;equipment[5]=CloudBoots;equipment[7]=GlowCharm;
      for (int f=0;f<art::HumanFrames;++f) for (int h:held) for(bool face:{false,true})
        for (bool active:{false,true}) for (int light:{28,112,255}) {
          starsoil_actor::Appearance a{32,48,f,1,light,outfit%3,face,active,equipment,uint8_t(h)};
          starsoil_actor::draw(a,[&](int x,int y,uint16_t c,starsoil_actor::Tag t,uint8_t index){
            uint64_t value=unsigned(x)|(uint64_t(unsigned(y))<<8)|(uint64_t(c)<<16)|(uint64_t(t)<<32)|(uint64_t(index)<<40);
            fingerprint=(fingerprint^value)*1099511628211ull;
          });
        }
    }
  return fingerprint;
}
int main(int argc,char **argv) {
  check_encoded_images<art::humans>();
  check_encoded_images<art::mail>();
  check_encoded_images<art::outfits>();
  check_encoded_images<art::helmets>();
  puts("PASS every packed character image restores the original indexed bitmap and painter order");
  auto fingerprint=run(1);
  if(argc==2 && std::strcmp(argv[1],"--print-fingerprint")==0) {
    std::printf("0x%llx\n",static_cast<unsigned long long>(fingerprint));return 0;
  }
  assert(fingerprint == 0x277d2246f3d0103bull);
  puts("PASS 2880 reviewed actor plot streams: clothes, armor, held tools, both facings, walk/stand/ascent/apex/descent and lighting");
}
