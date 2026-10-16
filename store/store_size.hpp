#pragma once

#include <cstdint>
#include <cstdio>
#include <span>

namespace store {

// The byte count is the complete downloaded .pxa container for this device.
// Binary units are explicit; rounding must not silently truncate the download.
inline void format_package_size(std::span<char> output, std::uint64_t bytes) {
    constexpr const char *units[] = {"KiB", "MiB", "GiB", "TiB"};
    if (bytes < 1024) {
        std::snprintf(output.data(), output.size(), "%llu B",
                      static_cast<unsigned long long>(bytes));
        return;
    }
    std::uint64_t unit = 1024;
    unsigned index = 0;
    while (index < 3 && bytes / unit >= 1024) {
        unit *= 1024;
        ++index;
    }
    auto whole = bytes / unit;
    auto fraction = ((bytes % unit) * 100 + unit / 2) / unit;
    if (fraction == 100) { ++whole; fraction = 0; }
    if (whole == 1024 && fraction == 0 && index < 3) {
        whole = 1;
        ++index;
    }
    std::snprintf(output.data(), output.size(), "%llu.%02u %s",
                  static_cast<unsigned long long>(whole),
                  static_cast<unsigned>(fraction), units[index]);
}

inline void format_package_bytes(std::span<char> output, std::uint64_t bytes) {
    std::snprintf(output.data(), output.size(), "%llu B",
                  static_cast<unsigned long long>(bytes));
}

} // namespace store
