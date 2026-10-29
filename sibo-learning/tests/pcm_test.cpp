// SPDX-License-Identifier: MIT
#include "../pcm_convert.hpp"
#include <cassert>
#include <cmath>
#include <iostream>
#include <vector>
using namespace sibo;
static std::vector<int16_t> convert(const std::vector<int16_t> &input,
                                    unsigned source, unsigned target,
                                    unsigned channels, unsigned chunk) {
  PcmConvert converter;
  converter.configure(source, target, channels);
  media::PcmPacket packet;
  packet.configure(target, 1);
  std::vector<int16_t> output;
  auto drain = [&] {
    output.insert(output.end(), packet.samples, packet.samples + packet.frames);
    packet.frames = 0;
  };
  for (unsigned at = 0; at < input.size() / channels;) {
    unsigned count = std::min(chunk, unsigned(input.size() / channels - at)),
             used = 0;
    while (used < count || converter.pending) {
      used += converter.append(input.data() + (at + used) * channels,
                               count - used, packet);
      if (packet.full())
        drain();
    }
    at += count;
  }
  converter.finish(packet);
  drain();
  return output;
}
int main() {
  for (auto source : {8000u, 16000u, 22050u, 24000u, 32000u, 44100u, 48000u})
    for (auto target : {16000u, 48000u})
      for (auto channels : {1u, 2u}) {
        std::vector<int16_t> input;
        const unsigned frames = 3501;
        for (unsigned i = 0; i < frames; ++i) {
          int16_t sample = int(i * 37 % 60000) - 30000;
          input.push_back(sample);
          if (channels == 2)
            input.push_back(sample * 3 / 5);
        }
        auto expected = convert(input, source, target, channels, 1152);
        assert(expected.size() ==
               (uint64_t(frames) * target + source - 1) / source);
        // Independent integration over continuous source sample intervals.
        for (unsigned j = 0; j < expected.size(); ++j) {
          double start = double(j) * source / target,
                 end =
                     std::min<double>(frames, double(j + 1) * source / target),
                 sum = 0;
          for (unsigned i = unsigned(start); i < std::ceil(end); ++i) {
            int sample = input[i * channels];
            if (channels == 2)
              sample = (sample + input[i * channels + 1]) / 2;
            sum += sample *
                   (std::min(end, double(i + 1)) - std::max(start, double(i)));
          }
          int reference = int(sum / (end - start));
          assert(std::abs(reference - int(expected[j])) <= 1);
        }
        for (auto chunk : {1u, 37u, 576u})
          assert(convert(input, source, target, channels, chunk) == expected);
        std::vector<int16_t> dc(3501 * channels, 1234);
        auto constant = convert(dc, source, target, channels, 37);
        for (auto s : constant)
          assert(s == 1234);
      }
  std::cout << "PCM fallback: mono/stereo, seven rates, exact duration and "
               "MP3/packet boundaries passed\n";
}
