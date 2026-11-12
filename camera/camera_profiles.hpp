// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <cstdint>
#include <cstddef>
#include <optional>
#include <span>

namespace camera_profiles {
inline bool supports_window_orientation(uint32_t width, uint32_t height, uint8_t rotations, bool portrait) {
    if (!width || !height) return false;
    for (unsigned turn = 0; turn < 4; ++turn)
        if ((rotations & (1u << turn)) && (width == height || ((height > width) != bool(turn & 1)) == portrait))
            return true;
    return false;
}
template<class Profile>
bool matches(const Profile& profile, bool portrait) {
    return profile.width && profile.height &&
        (profile.width == profile.height || (profile.height > profile.width) == portrait);
}
template<class Profile>
std::optional<std::size_t> orient(std::span<const Profile> profiles, std::size_t current, bool portrait) {
    if (current >= profiles.size()) return std::nullopt;
    const auto& source = profiles[current];
    if (matches(source, portrait)) return current;
    std::optional<std::size_t> best;
    uint64_t best_area = UINT64_MAX;
    unsigned best_quality = UINT32_MAX;
    const uint64_t source_area = uint64_t(source.width) * source.height;
    for (std::size_t i = 0; i < profiles.size(); ++i) {
        const auto& candidate = profiles[i];
        if (!matches(candidate, portrait)) continue;
        if (candidate.width == source.height && candidate.height == source.width &&
            candidate.quality == source.quality) return i;
        const uint64_t area = uint64_t(candidate.width) * candidate.height;
        const auto difference = area > source_area ? area-source_area : source_area-area;
        const unsigned quality = candidate.quality > source.quality ?
            candidate.quality-source.quality : source.quality-candidate.quality;
        if (!best || difference < best_area || (difference == best_area && quality < best_quality)) {
            best = i; best_area = difference; best_quality = quality;
        }
    }
    return best;
}
}
