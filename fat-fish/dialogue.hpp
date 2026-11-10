// SPDX-License-Identifier: MIT
#pragma once
#include "layout.hpp"
#include "../common/pocket-ui/i18n.hpp"

namespace fish {
inline int dialogue_width(const char *text, int unit = 12) {
    int width = 0;
    while (*text) width += pocket_ui::next_utf8(text) < 128 ? std::max(1, unit * 7 / 12) : unit;
    return width;
}
struct Dialogue {
    char lines[3][128]{};
    unsigned count = 0;
    int width = 0;
};
inline Dialogue wrap_dialogue(const char *text, int width, int unit = 12, unsigned max_lines = 3) {
    Dialogue result;
    width = std::max(36, width);
    max_lines = std::clamp(max_lines, 1u, 3u);
    while (*text && result.count < max_lines) {
        while (*text == ' ') ++text;
        if (!*text) break;
        const char *start = text, *end = text, *space = nullptr;
        const int limit = result.count + 1 == max_lines ? width - std::max(24, unit * 7 / 12 * 3) : width;
        int pixels = 0;
        while (*end && *end != '\n') {
            const char *next = end;
            const uint32_t cp = pocket_ui::next_utf8(next);
            const int advance = cp < 128 ? std::max(1, unit * 7 / 12) : unit;
            if (pixels + advance > limit || next - start >= 124) break;
            if (cp == ' ') space = end;
            pixels += advance;
            end = next;
        }
        const char *next = *end == '\n' ? end + 1 : end;
        if (*end && *end != '\n' && *end != ' ' && space) { end = space; next = space + 1; }
        while (end > start && end[-1] == ' ') --end;
        auto &line = result.lines[result.count++];
        memcpy(line, start, size_t(end - start));
        text = next;
        if (result.count == max_lines && *text) strcat(line, "...");
        result.width = std::max(result.width, dialogue_width(line, unit));
    }
    return result;
}
struct SpeechBubble {
    int x, y, width, height, tail_x;
};
inline SpeechBubble bubble_layout(int canvas_width, const RoomLayout &room, int position,
                                 int bottom, const Dialogue &text, int line_height = 13) {
    const int width = std::clamp(text.width + 43, 110, std::min(218, canvas_width - 24));
    const int height = std::min(int(text.count) * line_height + 17, std::max(18, bottom - 54));
    const int center = room.foot_x(position);
    const int head = room.pet_y() + room.pet_height * 25 / 144;
    const int x = std::clamp(center + room.pet_height / 3 - width / 2, 12, canvas_width - width - 12);
    const int y = std::clamp(head - height - 8, 44, bottom - height - 10);
    return {x, y, width, height, std::clamp(center + 8, x + 18, x + width - 18)};
}
} // namespace fish
