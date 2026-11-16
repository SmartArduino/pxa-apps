#pragma once
#include "browser_core.hpp"

namespace browser::image_response {
// Keep the Host's encoded-input budget separate from the pixel-cache budget.
inline constexpr std::uint32_t encoded_limit = 512 * 1024;
inline constexpr std::uint32_t compressed_limit = 512 * 1024;
inline std::string mime(std::string_view value) {
    return lowercase(trim(value.substr(0, value.find(';'))));
}
inline bool declared_image(std::string_view value) {
    return mime(value).starts_with("image/");
}
// Passive images on a secure page may use their secure endpoint. Never fall
// back to plaintext if that endpoint fails; redirects retain the same rule.
inline std::string request_url(std::string_view page, std::string target) {
    if (page.starts_with("https:") && target.starts_with("http://")) {
        const auto end = target.find_first_of("/?#", 7);
        const auto authority = std::string_view(target).substr(7, end == target.npos ? end : end-7);
        if (authority.ends_with(":80")) target.erase(7+authority.size()-3, 3);
        target.replace(0, 4, "https");
    }
    return target;
}
}
