// SPDX-License-Identifier: MIT
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
namespace motion {
inline void put32(std::byte* p,uint32_t n){for(unsigned i=0;i<4;++i)p[i]=std::byte(n>>(i*8));}
inline uint32_t get32(const std::byte* p){uint32_t n=0;for(unsigned i=0;i<4;++i)n|=uint32_t(p[i])<<(i*8);return n;}
inline uint32_t checksum(std::span<const std::byte> bytes){uint32_t n=2166136261;for(auto b:bytes)n=(n^uint8_t(b))*16777619;return n;}
}
