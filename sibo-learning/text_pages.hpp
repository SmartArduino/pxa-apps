// SPDX-License-Identifier: MIT
#pragma once
#include <algorithm>
#include <string>
#include <string_view>
#include <vector>
namespace sibo {
// Half-em cells. Permit one-line pages on tiny screens and more than eight
// lines on tablets; preserve all original UTF-8 bytes without slicing a glyph.
inline std::vector<std::string> text_pages(std::string_view value,
                                         unsigned columns, unsigned rows) {
  columns=std::clamp(columns,2u,512u);rows=std::clamp(rows,1u,64u);
  std::vector<std::string> pages;
  size_t begin=0;
  while(begin<value.size()) {
    size_t at=begin;unsigned line=0,used=0;
    while(at<value.size()) {
      const auto c=static_cast<unsigned char>(value[at]);
      auto bytes=std::min<size_t>(c<128?1:c<224?2:c<240?3:4,value.size()-at);
      const unsigned cells=c>=128 || c=='W' || c=='M' || c=='\t' ? 2 : 1;
      if(c=='\n') { at+=bytes;if(++line>=rows)break;used=0;continue; }
      if(used+cells>columns) { if(++line>=rows)break;used=0; }
      used+=cells;at+=bytes;
    }
    if(at==begin)at=begin+std::min<size_t>(value.size()-begin,1);
    // Avoid splitting a Latin word, while always making progress.
    if(at<value.size() && static_cast<unsigned char>(value[at])<128 &&
       value[at]!=' ' && value[at]!='\n') {
      auto space=value.rfind(' ',at-1);
      if(space!=value.npos && space>begin && at-space<20)at=space+1;
    }
    pages.emplace_back(value.substr(begin,at-begin));begin=at;
  }
  if(pages.empty())pages.emplace_back();
  return pages;
}
} // namespace sibo
