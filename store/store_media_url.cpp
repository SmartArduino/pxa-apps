#include "store_media.hpp"
#include "store_model.h"
#include "store_client.h"
#include <cstring>

namespace store {
bool media_url(std::string_view path, char *output, unsigned capacity) {
    constexpr std::string_view origin = PXA_STORE_ORIGIN;
    if (path.starts_with(origin) && path.size() > origin.size() &&
        path[origin.size()] == '/') path.remove_prefix(origin.size());
    if (!path.starts_with("/media/apps/") || path.size() >= STORE_MAX_MEDIA_URL ||
        path.find("..") != path.npos || path.find('%') != path.npos ||
        path.find('#') != path.npos || path.find('\\') != path.npos ||
        origin.size() + path.size() + 1 > capacity) return false;
    for (unsigned char c : path) if (c < 0x21 || c > 0x7e) return false;
    std::memmove(output + origin.size(), path.data(), path.size());
    std::memcpy(output, origin.data(), origin.size());
    output[origin.size() + path.size()] = 0;
    return true;
}
} // namespace store
