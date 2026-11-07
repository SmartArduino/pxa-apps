// SPDX-License-Identifier: MIT
#pragma once
#include <cstdint>
#include <algorithm>
namespace dewfield_pacing {
inline constexpr uint64_t PeriodUs = 33333;
inline uint64_t wait_us(uint64_t now, uint64_t tick_started,
                        uint64_t frame_started, bool frame_pending) {
  uint64_t deadline = tick_started + PeriodUs;
  const uint64_t present_at = frame_started + PeriodUs;
  // Acquiring a mapped buffer puts frame_started slightly after the tick.
  // Wake at that remaining deadline instead of skipping to the next tick.
  // A due-but-blocked frame waits for a release event or the next tick; it
  // must not spin on a past presentation deadline.
  if (frame_pending && present_at > now) deadline = std::min(deadline,present_at);
  return now < deadline ? deadline-now : 0;
}
} // namespace dewfield_pacing
