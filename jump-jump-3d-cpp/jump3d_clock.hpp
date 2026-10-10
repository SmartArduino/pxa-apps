#pragma once
#include <cstdint>

namespace jump {
// Keep the original game's per-callback rounding and two-step catch-up cap.
// An accumulating fixed-step clock changes charge/flight timing when callbacks
// arrive between 30 and 40 ms apart, even with identical gameplay constants.
class GameClock {
public:
    void reset() noexcept { previous_us_ = 0; }
    std::uint64_t previous_timestamp() const noexcept { return previous_us_; }
    std::uint8_t advance(std::uint64_t timestamp_us) noexcept {
        if (!timestamp_us || timestamp_us <= previous_us_) return 0;
        const auto previous = previous_us_;
        previous_us_ = timestamp_us;
        if (!previous) return 1;
        const auto steps = (timestamp_us - previous + step_us / 2) / step_us;
        return static_cast<std::uint8_t>(steps > maximum_steps ? maximum_steps : steps);
    }
    static constexpr std::uint32_t step_us = 20000;
    static constexpr std::uint8_t maximum_steps = 2;
private:
    std::uint64_t previous_us_ = 0;
};
}
