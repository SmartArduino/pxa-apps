// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <algorithm>
#include <array>
#include <string>
#include <string_view>

namespace xiaozhi {
inline int caption_advance(unsigned char c, int font) {
  return c < 128 ? std::max(4, font * 3 / 5) : font;
}
inline std::array<std::string, 3> wrap_caption(std::string_view text, int width,
                                             int font, unsigned max_lines) {
  std::array<std::string, 3> out;
  if (!max_lines || width <= 0 || font <= 0)
    return out;
  max_lines = std::min(max_lines, 3u);
  unsigned line = 0;
  int used = 0;
  for (std::size_t at = 0; at < text.size();) {
    auto c = static_cast<unsigned char>(text[at]);
    if (c == '\r') { ++at; continue; }
    std::size_t n = c < 128 ? 1 : (c & 0xe0) == 0xc0 ? 2 : (c & 0xf0) == 0xe0 ? 3 : 4;
    n = std::min(n, text.size() - at);
    const int advance = caption_advance(c, font);
    if (c == '\n' || used + advance > width) {
      if (line + 1 >= max_lines) {
        if (c == '\n' && at + 1 == text.size()) break;
        auto &last = out[line];
        while (!last.empty() && used + font > width) {
          std::size_t start = last.size() - 1;
          while (start && (static_cast<unsigned char>(last[start]) & 0xc0) == 0x80) --start;
          used -= caption_advance(static_cast<unsigned char>(last[start]), font);
          last.resize(start);
        }
        if (font <= width) last += "…";
        break;
      }
      ++line;
      used = 0;
      if (c == '\n') { ++at; continue; }
    }
    out[line].append(text.substr(at, n));
    used += advance;
    at += n;
  }
  return out;
}
inline unsigned caption_line_count(const std::array<std::string, 3> &lines) {
  unsigned count = 0;
  for (unsigned i = 0; i < lines.size(); ++i)
    if (!lines[i].empty()) count = i + 1;
  return count;
}
} // namespace xiaozhi
