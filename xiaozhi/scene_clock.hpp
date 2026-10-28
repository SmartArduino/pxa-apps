// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <cstdint>
namespace xiaozhi {
// Host Clock provides UTC and its current local offset (including DST).
// The normalized modulo also handles local dates before the Unix epoch.
constexpr unsigned scene_for_local_seconds(std::int64_t local_seconds) {
  const unsigned hour =
      unsigned((local_seconds % 86400 + 86400) % 86400) / 3600;
  if (hour >= 5 && hour < 9)
    return 0; // Morning.
  if (hour >= 9 && hour < 17)
    return 1; // Day.
  if (hour >= 17 && hour < 20)
    return 2; // Evening.
  return 3;   // Night.
}
} // namespace xiaozhi
