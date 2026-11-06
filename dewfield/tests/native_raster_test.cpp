// SPDX-License-Identifier: MIT
#include "pxa_control_capture.h"
#include "../native_raster.hpp"
#include <cassert>
#include <vector>
static int32_t failure;
static unsigned calls;
static unsigned commands;
static uint16_t *output;
static int width, height;
extern "C" int32_t pxa_test_control(const uint8_t *, uint32_t) { return 0; }
extern "C" int32_t pxa_io(uint64_t handle, uint32_t operation, uint8_t *packet, uint32_t bytes) {
  assert(handle == 77 && operation == 0x10d); ++calls;
  if (failure) return failure;
  pxa_game_render_rgb565_header_wire_t header{};
  assert(pxa_game_render_rgb565_header_decode(packet, bytes, &header));
  assert(header.target_width == width && header.target_height == height);
  assert(header.count <= 256 && bytes == 32 + header.count * 32);
  commands = header.count;
  for (unsigned i = 0; i < header.count; ++i) {
    pxa_game_render_rgb565_command_wire_t c{};
    assert(pxa_game_render_rgb565_command_decode(packet + 32 + i * 32, 32, &c));
    assert(c.kind == 0 && c.light == 255);
    for (int y = c.y; y < c.y + c.height; ++y)
      for (int x = c.x; x < c.x + c.width; ++x) {
        assert(x >= 0 && y >= 0 && x < width && y < height);
        output[y * width + x] = c.color;
      }
  }
  return bytes;
}
int main() {
  width = 83; height = 61;
  for (int status : {0, PXA_STATUS_WOULD_BLOCK, PXA_STATUS_UNSUPPORTED}) {
    dewfield_raster::NativeRaster raster;
    raster.created({0,77,256}); calls = 0; failure = status;
    std::vector<uint16_t> actual(width*height,0x55aa), expected = actual;
    output = actual.data(); raster.begin(output,width,height);
    uint32_t rng = 19;
    for (int i = 0; i < 1800; ++i) {
      rng = rng * 1664525 + 1013904223;
      int x = int(rng % 113) - 15, y = int(rng / 113 % 91) - 15;
      int w = 1 + rng % 31, h = 1 + rng % 17;
      auto color = uint16_t(rng >> 16);
      assert(raster.fill(x,y,w,h,color));
      for (int yy = std::max(y,0); yy < std::min(y+h,height); ++yy)
        for (int xx = std::max(x,0); xx < std::min(x+w,width); ++xx)
          expected[yy*width+xx] = color;
      if (i % 37 == 0) { raster.flush(); assert(actual == expected); }
    }
    raster.end(); assert(actual == expected); assert(calls);
    if (status == PXA_STATUS_UNSUPPORTED) assert(calls == 1);
    if (status == PXA_STATUS_WOULD_BLOCK) assert(calls > 1);
    // No queueing outside the explicitly bounded background section.
    assert(!raster.fill(0,0,1,1,0));
  }
  // Several queue rollovers and the two merge directions preserve coverage.
  dewfield_raster::NativeRaster raster; raster.created({0,77,256}); failure=0; calls=0;
  std::vector<uint16_t> actual(width*height), expected = actual; output=actual.data();
  raster.begin(output,width,height);
  assert(raster.fill(0,0,41,1,0x1357));
  assert(raster.fill(41,0,42,1,0x1357));
  assert(raster.fill(0,1,83,2,0x1357));
  raster.end(); assert(commands == 1);
  for(int i=0;i<width*height;++i) assert(actual[i] == (i < 3*width ? 0x1357 : 0));
  raster.begin(output,width,height);
  for(int y=0;y<height;++y) for(int x=0;x<width;++x) {
    auto c = uint16_t((x+y)%2 ? 0x1234 : 0x4321);
    assert(raster.fill(x,y,1,1,c)); expected[y*width+x]=c;
  }
  raster.end(); assert(calls > 10 && actual==expected);
  // Unsupported/busy operations both preserve the exact software output.
  std::vector<uint16_t> source(17*13);
  for(unsigned i=0;i<source.size();++i) source[i]=uint16_t(i*251);
  for(int status : {PXA_STATUS_WOULD_BLOCK, PXA_STATUS_UNSUPPORTED}) {
    dewfield_raster::NativeRaster sampler; sampler.created({0,77,256});
    failure=status; calls=0;
    for(int pass=0;pass<2;++pass) {
      source[0]^=0xffff;
      sampler.resample(source.data(),17,13,actual.data(),width,height);
      for(int y=0;y<height;++y) for(int x=0;x<width;++x)
        assert(actual[y*width+x] == source[(y*13/height)*17+x*17/width]);
    }
    assert(calls == (status==PXA_STATUS_UNSUPPORTED ? 1u : 2u));
  }
}
