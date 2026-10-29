// SPDX-License-Identifier: MIT
#pragma once
#include <algorithm>
#include <cstdint>
namespace sibo {
struct Layout {
  unsigned width = 240, height = 284;
  unsigned physical_width = 240, physical_height = 284;
  bool roomy = false, sidebar = false;
  unsigned margin = 11, header = 34, content_width = 218;
  unsigned list_columns = 1, page_size = 6, home_columns = 1;
  unsigned sidebar_width = 0, home_width = 218, home_tile_height = 64;
  unsigned hero_width = 218, cover_width = 218, cover_height = 150;
  unsigned picture_toolbar_height = 36;
  unsigned pinyin_columns = 4, pinyin_rows = 2, pinyin_cell = 44;
  unsigned reader_unit = 10, reader_line = 26, reader_rows = 4;
  Layout() = default;
  Layout(unsigned w, unsigned h, unsigned pixels_w = 0, unsigned pixels_h = 0)
      : width(w), height(h), physical_width(pixels_w ? pixels_w : w),
        physical_height(pixels_h ? pixels_h : h) {
    roomy = std::min(physical_width, physical_height) >= 300;
    sidebar = roomy && w >= 600 && uint64_t(w) * 10 > uint64_t(h) * 11;
    margin = roomy ? (w >= 420 ? 16 : 12) : 11;
    header = roomy ? 44 : 34;
    content_width = w > 2 * margin ? w - 2 * margin : 120;
    list_columns = roomy ? (w >= 1100 ? 4 : w >= 720 ? 3 : 2) : 1;
    auto cell_width = (content_width - (list_columns - 1) * 8) / list_columns;
    auto rows = std::clamp(
        (h > 160 ? h - 160 : 160) / (cell_width < 180 ? 96 : 80), 2u, 8u);
    page_size = roomy ? std::min(24u, list_columns * rows) : 6;
    sidebar_width = sidebar ? std::clamp(content_width / 3, 180u, 280u) : 0;
    home_width = sidebar ? content_width - sidebar_width - 12 : content_width;
    home_columns = roomy ? (sidebar && home_width >= 650 ? 3 : 2) : 1;
    const unsigned home_rows = (8 + home_columns - 1) / home_columns;
    auto available =
        h > header + 2 * margin + 7 ? h - header - 2 * margin - 7 : 200;
    if (!sidebar)
      available = available > 84 ? available - 84 : 180;
    home_tile_height =
        std::clamp((available - (home_rows - 1) * 8) / home_rows, 64u, 320u);
    hero_width = sidebar ? sidebar_width
                 : roomy ? 112
                         : std::clamp(content_width, 120u, 280u);
    cover_width = roomy ? std::min(content_width, 960u)
                        : std::clamp(content_width, 120u, 280u);
    picture_toolbar_height = roomy ? 56 : h < 250 ? 28 : 36;
    const auto reserved =
        header + 7 + 2 * margin + 2 * picture_toolbar_height + 16;
    cover_height = h > reserved ? std::max(48u, h - reserved) : 48;
    pinyin_columns =
        std::clamp((w > 36 ? w - 36 : 120) / 45, 3u, roomy ? 8u : 6u);
    pinyin_rows = std::clamp((h > 150 ? h - 150 : 58) / 58, 1u,
                             roomy ? 32 / pinyin_columns : 3u);
    pinyin_cell =
        roomy ? (content_width - (pinyin_columns - 1)) / pinyin_columns : 44;
    reader_unit = std::min(physical_width, physical_height) >= 420 ? 14 : 10;
    reader_line = reader_unit == 14 ? 42 : roomy ? 30 : 26;
    reader_rows =
        roomy     ? std::max(3u, (h - header - 2 * margin - 105) / reader_line)
        : h > 166 ? (h - 166) / 26
                  : 3;
  }
};
} // namespace sibo
