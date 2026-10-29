// SPDX-License-Identifier: MIT
#pragma once
#include "../common/media/pcm_packet.hpp"
#include <algorithm>
#include <cstdint>
namespace sibo {
// Streaming area resampling for legacy mono sinks. Integer weights preserve
// duration, DC level and phase across MP3 frames and Host packet boundaries.
struct PcmConvert {
  unsigned input_rate = 0, output_rate = 0, channels = 0;
  unsigned remaining = 0, pending = 0;
  int32_t sample = 0;
  int64_t sum = 0;
  void configure(unsigned source, unsigned target, unsigned count) {
    input_rate = source;
    output_rate = target;
    channels = count;
    remaining = source;
    pending = 0;
    sum = 0;
  }
  unsigned append(const int16_t *input, unsigned count, media::PcmPacket &out) {
    unsigned consumed = 0;
    while (!out.full() && (pending || consumed < count)) {
      if (!pending) {
        sample = input[consumed * channels];
        if (channels == 2)
          sample = (sample + input[consumed * channels + 1]) / 2;
        ++consumed;
        pending = output_rate;
      }
      auto weight = std::min(pending, remaining);
      sum += int64_t(sample) * weight;
      pending -= weight;
      remaining -= weight;
      if (!remaining) {
        out.samples[out.frames++] = int16_t(sum / input_rate);
        sum = 0;
        remaining = input_rate;
      }
    }
    return consumed;
  }
  void finish(media::PcmPacket &out) {
    if (!pending && remaining < input_rate && !out.full()) {
      out.samples[out.frames++] = int16_t(sum / (input_rate - remaining));
      sum = 0;
      remaining = input_rate;
    }
  }
};
} // namespace sibo
