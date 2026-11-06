// SPDX-License-Identifier: MIT
#include "../resample.hpp"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <utility>
#include <vector>
int main() {
  // Compare to the mathematical mapping, including odd 2x widths and resize
  // transitions. Guard both ends to catch wide stores at row boundaries.
  for (auto source : {std::pair<int, int>{1, 1},
                      {7, 5},
                      {240, 240},
                      {239, 137},
                      {320, 240},
                      {480, 320}})
    for (auto target : {std::pair<int, int>{2, 2},
                        {14, 10},
                        {478, 274},
                        {480, 480},
                        {320, 240},
                        {240, 320},
                        {97, 73},
                        {480, 320}}) {
      int sw = source.first, sh = source.second, w = target.first,
          h = target.second;
      std::vector<uint16_t> input(sw * sh), output(w * h + 32, 0xabcd);
      for (int i = 0; i < sw * sh; ++i)
        input[i] = uint16_t(i * 197 + 31);
      dewfield_raster::resample(input.data(), sw, sh, output.data() + 16, w, h);
      for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
          assert(output[16 + y * w + x] ==
                 input[(y * sh / h) * sw + x * sw / w]);
      for (int i = 0; i < 16; ++i) {
        assert(output[i] == 0xabcd);
        assert(output[output.size() - 1 - i] == 0xabcd);
      }
    }
  for (auto target : {std::pair<int, int>{480, 480},
                      {320, 240},
                      {480, 320},
                      {240, 320},
                      {320, 480},
                      {479, 317}}) {
    constexpr int sw = 240, sh = 160;
    int w = target.first, h = target.second;
    std::vector<uint16_t> input(sw * sh), output(w * h + 32, 0xabcd);
    for (int i = 0; i < sw * sh; ++i)
      input[i] = uint16_t(i);
    dewfield_raster::cover(input.data(), sw, sh, output.data() + 16, w, h);
    // Independent continuous image geometry: a single zoom factor and a
    // centred viewport. Exact 3x square-screen blocks also follow from this.
    double zoom = std::max(double(w) / sw, double(h) / sh);
    for (int y = 0; y < h; ++y)
      for (int x = 0; x < w; ++x) {
        int sx = int(std::floor((x + .5 - w / 2.) / zoom + sw / 2. + 1e-9));
        int sy = int(std::floor((y + .5 - h / 2.) / zoom + sh / 2. + 1e-9));
        assert(output[16 + y * w + x] == input[sy * sw + sx]);
      }
    for (int i = 0; i < 16; ++i) {
      assert(output[i] == 0xabcd);
      assert(output[output.size() - 1 - i] == 0xabcd);
    }
  }
  puts("PASS: exact nearest-neighbour pixels, uniform cover/cropping, square/"
       "landscape/portrait viewports and guarded output");
}
