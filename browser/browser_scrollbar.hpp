#pragma once
#include <algorithm>
#include <cstdint>

namespace browser {
// All values are physical pixels. No cached bitmap or per-frame allocation.
struct Scrollbar {
    int top, height, total, position, minimum;
    int range() const { return std::max(0, total - height); }
    int thumb_height() const {
        return int(std::min<std::int64_t>(height, std::max<std::int64_t>(minimum,
            std::int64_t(height) * height / std::max(1, total))));
    }
    int thumb_top() const {
        return top + int(std::int64_t(std::clamp(position, 0, range())) *
                         (height - thumb_height()) / std::max(1, range()));
    }
    int position_at(int y, int offset) const {
        const int travel = height - thumb_height();
        if (travel <= 0) return 0;
        const auto relative = std::clamp(std::int64_t(y) - offset - top,
                                          std::int64_t(0), std::int64_t(travel));
        return int((relative * range() + travel / 2) / travel);
    }
};
} // namespace browser
