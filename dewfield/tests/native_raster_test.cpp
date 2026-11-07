// SPDX-License-Identifier: MIT
#include "pxa_control_capture.h"
#include "../native_raster.hpp"
#include <cassert>
#include <vector>
static int32_t failure;
static unsigned calls;
static unsigned commands;
static uint16_t *output;
static const uint16_t *copy_source;
static unsigned copy_available;
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
    if (c.kind == PXA_GAME_RENDER_RGB565_BLIT) {
      assert(copy_source && c.light == 255 && header.count == 1);
      assert(c.source_offset == uint32_t(uintptr_t(copy_source)));
      assert(c.source_bytes <= copy_available * sizeof(uint16_t));
      assert(c.source_pitch >= c.source_width * sizeof(uint16_t));
      assert(c.source_bytes >= (c.source_height - 1) * c.source_pitch +
                                  c.source_width * sizeof(uint16_t));
      for (int y = 0; y < c.height; ++y)
        for (int x = 0; x < c.width; ++x) {
          assert(x < width && y < height);
          output[y * width + x] = copy_source[
              (y * c.source_height / c.height) * (c.source_pitch / 2) +
              x * c.source_width / c.width];
        }
      continue;
    }
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
  // A cropped cache view has row padding and can end near its allocation's
  // last pixel. Its wire range must fit, and copying must preserve the rest
  // of the mapped canvas and both allocation guards.
  std::vector<uint16_t> cache(11 * 7), guarded(width * height + 6, 0x55aa);
  for (unsigned i = 0; i < cache.size(); ++i) cache[i] = uint16_t(i * 389);
  copy_source = cache.data() + 25;
  copy_available = unsigned(cache.size() - 25);
  output = guarded.data() + 3;
  for (int status : {0, PXA_STATUS_WOULD_BLOCK, PXA_STATUS_UNSUPPORTED}) {
    std::fill(guarded.begin(), guarded.end(), 0x55aa);
    dewfield_raster::NativeRaster copier; copier.created({0,77,256});
    failure = status;
    assert(copier.copy(copy_source, 11, 7, 5, output, width, height) == !status);
    for (int y = 0; y < height; ++y)
      for (int x = 0; x < width; ++x)
        assert(output[y * width + x] ==
            (!status && x < 7 && y < 5 ? copy_source[y * 11 + x] : 0x55aa));
    for (int i = 0; i < 3; ++i)
      assert(guarded[i] == 0x55aa && guarded[guarded.size()-1-i] == 0x55aa);
  }
}
