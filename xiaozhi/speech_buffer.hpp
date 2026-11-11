// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <span>

namespace xiaozhi {
// Source-rate mono PCM. A bounded 480-ms reservoir absorbs packet jitter;
// only the first 120 ms are buffered before starting the Host's PCM FIFO.
class SpeechBuffer {
 public:
  static constexpr unsigned Capacity = 7680, Prebuffer = 1920;
  void reset() { head_ = used_ = 0; first_ms_ = 0; started_ = false; }
  unsigned available() const { return Capacity - used_; }
  unsigned size() const { return used_; }
  bool empty() const { return !used_; }
  bool push(std::span<const std::int16_t> pcm, std::uint64_t now) {
    if (pcm.size() > available()) return false;
    if (!used_ && !started_) first_ms_ = now;
    unsigned tail = (head_ + used_) % Capacity;
    for (auto sample : pcm) { data_[tail] = sample; tail = (tail + 1) % Capacity; }
    used_ += unsigned(pcm.size());
    return true;
  }
  bool ready(std::uint64_t now, bool finished) {
    if (!used_) return false;
    if (!started_ && (used_ >= Prebuffer || finished || now - first_ms_ >= 200))
      started_ = true;
    return started_;
  }
  std::span<const std::int16_t> front() const {
    return {data_.data() + head_, std::min({320u, used_, Capacity - head_})};
  }
  bool consume(unsigned samples) {
    if (samples > used_) return false;
    head_ = (head_ + samples) % Capacity; used_ -= samples; return true;
  }
 private:
  std::array<std::int16_t, Capacity> data_{};
  unsigned head_ = 0, used_ = 0;
  std::uint64_t first_ms_ = 0;
  bool started_ = false;
};
}
