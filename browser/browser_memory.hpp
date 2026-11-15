#pragma once
#include <algorithm>
#include <array>
#include <cstddef>
#include <string_view>
namespace browser::memory {
inline constexpr std::size_t min_heap_bytes=128*1024;
inline constexpr std::size_t max_heap_bytes=16*1024*1024;
constexpr bool valid_heap_limit(std::size_t bytes) noexcept {
    return bytes>=min_heap_bytes && bytes<=max_heap_bytes;
}
struct Choice { std::string_view label; std::size_t heap_bytes; };
inline constexpr std::array choices{
    Choice{"自动（按需，最多 1 MiB）",1024*1024}, Choice{"256 KiB",256*1024},
    Choice{"512 KiB",512*1024}, Choice{"1 MiB",1024*1024},
    Choice{"2 MiB",2*1024*1024}, Choice{"4 MiB",4*1024*1024},
    Choice{"8 MiB",8*1024*1024}, Choice{"16 MiB",16*1024*1024}};
constexpr std::size_t budget(unsigned choice, unsigned pressure = 0) noexcept {
    const auto selected = choices[choice < choices.size() ? choice : 0].heap_bytes;
    return pressure ? std::min(selected, std::size_t(256*1024)) : selected;
}
}
