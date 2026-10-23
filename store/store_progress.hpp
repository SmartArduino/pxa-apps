#pragma once

#include <cstdint>
#include <limits>

namespace store {

enum class DownloadPhase : std::uint8_t { waiting, program, resources, installing };

// The Host reports a single byte stream: program, then resources that need
// downloading. Reused resources are excluded from its actual transfer total.
inline DownloadPhase download_phase(bool started, std::uint64_t received,
                                    std::uint64_t total, std::uint64_t code_size) {
    if (!started) return DownloadPhase::waiting;
    if (total && received >= total) return DownloadPhase::installing;
    if (code_size && total > code_size && received >= code_size)
        return DownloadPhase::resources;
    return DownloadPhase::program;
}

inline std::uint8_t download_percent(std::uint64_t received, std::uint64_t total) {
    if (!total) return 0;
    if (received >= total) return 100;
    if (received <= std::numeric_limits<std::uint64_t>::max() / 100)
        return static_cast<std::uint8_t>(received * 100 / total);
    // Find the greatest whole percent reached, without overflowing uint64_t.
    unsigned low = 0, high = 100;
    while (low + 1 < high) {
        const auto mid = (low + high) / 2;
        const auto threshold = (total / 100) * mid + ((total % 100) * mid + 99) / 100;
        if (received >= threshold) low = mid;
        else high = mid;
    }
    return static_cast<std::uint8_t>(low);
}

} // namespace store
