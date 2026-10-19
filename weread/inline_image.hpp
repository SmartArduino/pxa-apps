#pragma once
#include <cstddef>
#include <optional>
#include <span>
#include <string_view>

namespace weread::inline_image {
inline constexpr std::string_view png_prefix = "data:image/png;base64,";
inline constexpr std::size_t max_uri_bytes = 4096, document_budget = 8192;
inline constexpr int digit(char c) noexcept {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    return c == '+' ? 62 : c == '/' ? 63 : -1;
}
// Strict, canonical Base64; validate before opening a temporary decode source.
inline std::optional<std::string_view> payload(std::string_view uri) noexcept {
    if (!uri.starts_with(png_prefix) || uri.size() > max_uri_bytes) return {};
    auto data = uri.substr(png_prefix.size());
    if (data.empty() || data.size() % 4) return {};
    std::size_t padding = data.ends_with("==") ? 2 : data.ends_with('=') ? 1 : 0;
    for (std::size_t i = 0; i < data.size() - padding; ++i) if (digit(data[i]) < 0) return {};
    if (padding && (digit(data[data.size()-padding-1]) & (padding == 2 ? 15 : 3))) return {};
    return data;
}
// Consume complete quartets into caller-owned scratch; no allocated container.
inline std::size_t decode(std::string_view data, std::span<std::byte> output) noexcept {
    std::size_t used = 0;
    for (std::size_t at = 0; at + 4 <= data.size(); at += 4) {
        const auto count = data[at+2] == '=' ? 1u : data[at+3] == '=' ? 2u : 3u;
        if (count > output.size() - used) break;
        const unsigned a = unsigned(digit(data[at])), b = unsigned(digit(data[at+1]));
        output[used++] = std::byte((a << 2) | (b >> 4));
        if (count > 1) output[used++] = std::byte((b << 4) | (unsigned(digit(data[at+2])) >> 2));
        if (count > 2) output[used++] = std::byte((unsigned(digit(data[at+2])) << 6) | unsigned(digit(data[at+3])));
    }
    return used;
}
} // namespace weread::inline_image
