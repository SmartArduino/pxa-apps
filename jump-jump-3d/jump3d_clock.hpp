#pragma once
#include <cstdint>

namespace jump {
// Advance by elapsed time, including coalesced callbacks. Rendering frequency
// must never change charge duration or slow down the ballistic trajectory.
class GameClock {
public:
    void reset() noexcept { previous_us_ = 0; }
    std::uint64_t previous_timestamp() const noexcept { return previous_us_; }
    float advance(std::uint64_t timestamp_us) noexcept {
        if (!timestamp_us || timestamp_us <= previous_us_) return 0;
        const auto previous = previous_us_;
        previous_us_ = timestamp_us;
        return previous ? static_cast<float>(timestamp_us - previous) * 1e-6F : 0;
    }
private:
    std::uint64_t previous_us_ = 0;
};
}
