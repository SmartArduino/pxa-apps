// SPDX-License-Identifier: MIT
#pragma once
#include "pxa_audio.h"
#include <algorithm>
#include <cstring>
namespace media {
// Prefer the media buffer when the Host supports Audio 0.16. Older Hosts
// reject the flag as an invalid argument. Some providers report unsupported.
struct PcmOpen {
  bool buffered = true;
  bool use_default(int32_t status) {
    if (!buffered ||
        (status != PXA_STATUS_UNSUPPORTED && status != PXA_STATUS_INVALID_ARGUMENT))
      return false;
    buffered = false;
    return true;
  }
  int32_t open(uint64_t token, uint64_t permission, unsigned rate,
               unsigned channels) {
    if (buffered) {
      int32_t status = pxa_audio_open_pcm_buffered(token, permission, rate, channels);
      if (!use_default(status))
        return status;
    }
    return pxa_audio_open_pcm(token, permission, rate, channels);
  }
};

// Fill each bounded Host packet across MP3 frame boundaries. Splitting every
// MP3 block into a full packet plus a short fragment wastes queue capacity.
struct PcmPacket {
  int16_t samples[1920]{};
  unsigned frames = 0, capacity = 0, channels = 0;
  void reset() { frames = capacity = channels = 0; }
  void configure(unsigned rate, unsigned count) {
    frames = 0;
    capacity = rate / 50;
    channels = count;
  }
  bool full() const { return capacity && frames == capacity; }
  unsigned append(const int16_t *data, unsigned count) {
    if (!capacity || !channels)
      return 0;
    unsigned n = std::min(count, capacity - frames);
    std::memcpy(samples + frames * channels, data,
                n * channels * sizeof(int16_t));
    frames += n;
    return n;
  }
  int write(uint64_t audio) {
    if (!frames)
      return 0;
    unsigned bytes = frames * channels * sizeof(int16_t);
    int n =
        pxa_audio_write_pcm(audio, reinterpret_cast<uint8_t *>(samples), bytes);
    if (n < 0)
      return n;
    if (unsigned(n) != bytes)
      return PXA_STATUS_PROTOCOL_ERROR;
    n = frames;
    frames = 0;
    return n;
  }
};
} // namespace media
