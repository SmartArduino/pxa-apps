#pragma once
#include "reader_core.hpp"
namespace reader {
// Replaces the old 16 KiB throwaway read with an 8 KiB reusable raw window.
// Displayed text remains separate and bounded by the Canvas command budget.
struct ReadingWindow {
  static constexpr std::size_t capacity = 8192, page_reserve = 4096, history_bytes = 2048;
  std::string text;
  std::uint32_t offset = 0;
  bool paragraph = true;
  void clear() {
    text.clear();
    offset = 0;
    paragraph = true;
  }
  bool covers(std::uint32_t position, std::uint32_t available) const {
    if (position < offset || position - offset >= text.size())
      return false;
    const auto remaining = text.size() - (position - offset);
    return remaining >= page_reserve || std::uint64_t(offset) + text.size() >= available;
  }
  std::string_view at(std::uint32_t position) const {
    return std::string_view(text).substr(position - offset);
  }
  bool paragraph_at(std::uint32_t position) const {
    if (position == offset)
      return paragraph;
    const auto c = text[position - offset - 1];
    return c == '\r' || c == '\n';
  }
};
inline void copy_page_text(std::string_view source, std::string &destination, Page &page) {
  destination.clear();
  for (std::size_t i = 0; i < page.count; ++i) {
    auto &line = page.lines[i];
    const auto begin = line.begin, end = line.end;
    line.begin = std::uint32_t(destination.size());
    for (auto at = begin; at < end; ++at)
      if (source[at] != '\r' && source[at] != '\n')
        destination += source[at];
    line.end = std::uint32_t(destination.size());
  }
}
} // namespace reader
