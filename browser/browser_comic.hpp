#pragma once
#include "browser_core.hpp"

namespace browser {
// A view over the existing page, not a second chapter/image list. Small UI
// icons and known unsupported formats do not become reader pages.
inline bool comic_image(const Image &image) {
    if ((image.declared_width && image.declared_width <= 64 &&
         image.declared_height && image.declared_height <= 64) ||
        (image.width <= 64 && image.height <= 64)) return false;
    if(srcset::unsupported(image.source))return false;
    return true;
}
inline bool comic_page(const Document &document, const Image &image) {
    return comic_image(image) && (document.comic_folder.empty() || image.source.starts_with(document.comic_folder));
}
inline std::pair<int, int> comic_position(const Document &document, int current) {
    int ordinal = 0, count = 0;
    for (std::size_t i = 0; i < document.images.size(); ++i)
        if (comic_page(document, document.images[i])) {
            ++count;
            if (int(i) == current) ordinal = count;
        }
    return {ordinal, count};
}
inline int comic_neighbor(const Document &document, int current, int direction) {
    if (direction != 1 && direction != -1) return -1;
    if (current < -1 || current > int(document.images.size())) return -1;
    for (int i = current + direction; i >= 0 && std::size_t(i) < document.images.size(); i += direction)
        if (comic_page(document, document.images[i])) return i;
    return -1;
}
inline constexpr std::uint32_t comic_detail_pixels = 262144;
} // namespace browser
