// SPDX-License-Identifier: MIT
#define PXA_POCKET_RESPONSIVE 1
#include "../../common/pocket-ui/viewport.hpp"
#include "../scene_resample.hpp"
#include "../ui_canvas.hpp"
#include <cassert>
#include <cstdio>
#include <vector>
int main() {
  for (auto src : {std::pair{299,224},std::pair{480,480},std::pair{288,224},std::pair{288,341},std::pair{288,288},std::pair{373,224},std::pair{7,11}})
    for (auto dst : {std::pair{320,240},std::pair{288,224},std::pair{480,480},std::pair{296,240},std::pair{240,284},std::pair{412,412},std::pair{400,240},std::pair{480,480},std::pair{11,7}}) {
      std::vector<uint16_t> source(src.first*src.second), expected(dst.first*dst.second),actual=expected;
      for (int pass=0;pass<2;++pass) {
        for(unsigned i=0;i<source.size();++i) source[i]=uint16_t(i*251+pass*531);
        pocket_ui::resample(source.data(),src.first,src.second,expected.data(),dst.first,dst.second);
        starsoil_scene::resample(source.data(),src.first,src.second,actual.data(),dst.first,dst.second);
        assert(actual==expected);
      }
    }
  // Alternate geometry and source contents on the SAME cached sampler. Check
  // the full output against the original algorithm, including both guards.
  starsoil_scene::Resampler sampler;
  uint32_t rng = 0x12345678;
  auto next = [&] { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return rng; };
  for (int pass = 0; pass < 120; ++pass) {
    int sw = 1 + next() % 480, sh = 1 + next() % 480;
    int w = 1 + next() % 480, h = 1 + next() % 480;
    std::vector<uint16_t> source(sw * sh), expected(w * h), guarded(w * h + 2, 0x1357);
    for (auto &pixel : source) pixel = uint16_t(next());
    for (int repeat = 0; repeat < 2; ++repeat) {
      source[0] ^= 0xffff;
      pocket_ui::resample(source.data(), sw, sh, expected.data(), w, h);
      sampler.draw(source.data(), sw, sh, guarded.data() + 1, w, h);
      assert(std::equal(expected.begin(), expected.end(), guarded.begin() + 1));
      assert(guarded.front() == 0x1357 && guarded.back() == 0x1357);
    }
  }
  for(int extent : {1,130,240,260,288,480}) for(int layout : {1,224,288,341,480})
    for(int v=-5000;v<=5000;++v)
      assert(starsoil_ui::Canvas::coordinate(v,extent,layout)==starsoil_ui::Canvas::ceil_div(int64_t(v)*extent,layout));
  puts("PASS exact scene resampling and repeated rows; exact signed UI coordinate rounding including fallback ranges");
}
