// SPDX-License-Identifier: MIT
#pragma once
#include <cstdint>
#include <algorithm>
namespace dewfield_pacing {
inline constexpr uint64_t PeriodUs = 33333;
template<class Tick,class Continue>
inline void advance(uint32_t elapsed_ms, Tick tick, Continue keep_going) {
  // Slow rendering must not turn 200 ms of real movement into a 100 ms step.
  // Keep collision, tool and portal checks in small steps, with bounded
  // catch-up after an exceptional stall or foreground transition.
  uint32_t remaining = std::min(elapsed_ms, 500u);
  while (remaining) {
    uint32_t step = std::min(remaining, 33u);
    tick(step);
    remaining -= step;
    if(!keep_going())break;
  }
}
template<class Tick>
inline void advance(uint32_t elapsed_ms,Tick tick) {
  advance(elapsed_ms,tick,[]{return true;});
}
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
