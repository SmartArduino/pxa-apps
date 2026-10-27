// SPDX-License-Identifier: MIT
#define MINIMP3_IMPLEMENTATION
#define MINIMP3_ONLY_MP3
#define MINIMP3_NO_SIMD
#include "decoder.hpp"
namespace media {
void Decoder::reset() { mp3dec_init(&state); }
int Decoder::decode(const uint8_t *p, int n, mp3dec_frame_info_t &info) {
  return mp3dec_decode_frame(&state, p, n, pcm, &info);
}
} // namespace media
