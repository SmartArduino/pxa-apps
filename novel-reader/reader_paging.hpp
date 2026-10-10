#pragma once
#include "reader_core.hpp"
#include <charconv>

namespace reader {
// Fixed sparse checkpoints, not an offset for every page. The app trades a
// portion of its existing back-page history for this optional accounting.
struct PageNumbers {
  struct Mark { std::uint32_t offset=0, number=1; };
  std::array<Mark,11> marks{};
  PaginationCursor cursor;
  std::uint32_t offset=0, pages=0, total=0, current=0,
      current_offset=UINT32_MAX, origin=0, generation=0, stride=65536,
      available=0, page_start=0, current_start=0;
  std::uint8_t mark_count=0;
  bool running=false, finished=false, exact_total=false, failed=false,
      current_boundary=false, stop_scan=false, go_previous=false;

  [[gnu::noinline]] void reset(std::uint32_t start=0) {
    const auto next_generation=generation+1;
    const bool was_running=running;
    *this={}; generation=next_generation; running=was_running;
    origin=offset=page_start=start;
  }
  [[gnu::noinline]] void locate(std::uint32_t target) {
    Mark anchor{origin,1};
    for (std::size_t i=0;i<mark_count;++i)
      if (marks[i].offset<=target && marks[i].offset>=anchor.offset) anchor=marks[i];
    offset=page_start=anchor.offset; pages=anchor.number-1;
    cursor={}; current=0; current_offset=UINT32_MAX; failed=false;
  }
  [[gnu::noinline]] void checkpoint() {
    if (mark_count && offset/stride<=marks[mark_count-1].offset/stride) return;
    if (mark_count==marks.size()) {
      for (std::size_t i=0;i<marks.size()/2;++i) marks[i]=marks[i*2];
      mark_count=marks.size()/2; stride*=2;
      if (offset/stride<=marks[mark_count-1].offset/stride) return;
    }
    marks[mark_count++]={offset,pages+1};
  }
  // Called synchronously while borrowing the shared I/O scratch buffer.
  [[gnu::noinline]] std::size_t consume(std::string_view text,const Layout& layout,
      const Settings& settings,std::uint32_t target,bool reflow,bool eof,bool focus) {
    std::size_t at=0;
    const int end=layout.body_end(), height=text_line_height(std::max(10,int(settings.font*layout.density)));
    while (at<text.size()) {
      if (!cursor.y && !cursor.lines && !focus) checkpoint();
      auto part=paginate_window(text.substr(at),layout,settings,cursor,reflow,eof);
      if (!part.consumed && !cursor.command_full) break;
      if (cursor.lines && target>=page_start && target<offset+part.consumed) {
        current=pages+1; current_offset=target; current_boundary=target==page_start;
        current_start=page_start;
      }
      offset+=std::uint32_t(part.consumed); at+=part.consumed;
      if (cursor.command_full || cursor.lines==64 || cursor.y+height>end) {
        const bool paragraph=at ? text[at-1]=='\n' || text[at-1]=='\r' : cursor.paragraph_start;
        ++pages; cursor={}; cursor.paragraph_start=paragraph; page_start=offset;
      }
      if (!focus) total=pages+(cursor.lines!=0);
      if (focus && current_offset==target) break;
      if (!part.count || !cursor.y) continue;
      break; // Re-read the uncommitted suffix with a fresh bounded window.
    }
    return at;
  }
  [[gnu::noinline]] std::string_view label(std::uint32_t position,std::span<char> scratch) const {
    char* end=scratch.data(); char* limit=scratch.data()+scratch.size();
    auto append=[&](std::string_view s) { if (std::size_t(limit-end)>=s.size()) end=std::copy(s.begin(),s.end(),end); };
    auto number=[&](std::uint32_t n) { end=std::to_chars(end,limit,n).ptr; };
    if (current && current_offset==position) number(current);
    else if (position==origin) number(1);
    else append("—");
    append(" / ");
    if (finished && exact_total) { number(total); append(" 页"); }
    else if (total) { append("已缓存 "); number(total); append("+ 页"); }
    else append("… 页");
    return {scratch.data(),std::size_t(end-scratch.data())};
  }
};
static_assert(sizeof(PageNumbers)<=160);
} // namespace reader
