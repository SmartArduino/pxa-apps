// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <cstdint>
namespace xiaozhi {
class ResponseWait {
public:
  enum class Phase { none, thinking };
  void think(std::uint64_t now) { phase_ = Phase::thinking; deadline_ = now + 30000; }
  void reset() { phase_ = Phase::none; deadline_ = 0; }
  Phase phase() const { return phase_; }
  bool expired(std::uint64_t now) const { return phase_ != Phase::none && now >= deadline_; }
private:
  Phase phase_ = Phase::none;
  std::uint64_t deadline_ = 0;
};
} // namespace xiaozhi
