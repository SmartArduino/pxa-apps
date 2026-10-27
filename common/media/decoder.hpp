// SPDX-License-Identifier: MIT
#pragma once
#include "vendor/minimp3.h"
namespace media {
struct Decoder {
  mp3dec_t state{};
  int16_t pcm[MINIMP3_MAX_SAMPLES_PER_FRAME]{};
  void reset();
  int decode(const uint8_t *, int, mp3dec_frame_info_t &);
};
} // namespace media
