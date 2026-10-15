#pragma once

#include <cstdint>
#include <span>
#include <string_view>

namespace store {
// Requests describe the current viewport, never the entire browsing history.
struct MediaRequest {
    std::string_view path;
    bool screenshot = false;
};
inline constexpr unsigned media_capacity = 8;

// Only same-origin /media/apps/ resources are accepted. No redirects, cookies,
// encoded image buffers or decoded pixels cross into Guest linear memory.
bool media_url(std::string_view path, char *output, unsigned capacity);
void media_start();
void media_update(std::span<const MediaRequest> visible, std::uint64_t permission);
std::uint64_t media_handle(std::string_view path, bool screenshot = false);
bool media_failed(std::string_view path, bool screenshot = false);
bool media_event(std::span<const std::byte> event);
bool media_changed();
void media_pause(bool paused);
void media_retry();
void media_stop();
} // namespace store
