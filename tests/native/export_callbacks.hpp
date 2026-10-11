#pragma once
#include <cstdint>
extern "C" {
std::int32_t pxa_app_start(const std::uint8_t*, std::uint32_t);
std::int32_t pxa_app_on_event(const std::uint8_t*, std::uint32_t);
void pxa_app_stop(std::uint32_t);
}
