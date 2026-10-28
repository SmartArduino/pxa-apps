// SPDX-License-Identifier: Apache-2.0
#include "../captions.hpp"
#include <cassert>
#include <cstdio>
int main() {
  auto blank = xiaozhi::wrap_caption("A\n\nB", 120, 12, 3);
  assert(blank[0] == "A" && blank[1].empty() && blank[2] == "B");
  assert(xiaozhi::caption_line_count(blank) == 3);
  auto chinese = xiaozhi::wrap_caption("你好世界", 30, 10, 1);
  assert(chinese[0] == "你好…");
  auto latin = xiaozhi::wrap_caption("abcd", 18, 10, 1);
  assert(latin[0] == "a…");
  auto trailing = xiaozhi::wrap_caption("你好\n", 30, 10, 1);
  assert(trailing[0] == "你好");
  auto crlf = xiaozhi::wrap_caption("A\r\nB", 120, 12, 3);
  assert(crlf[0] == "A" && crlf[1] == "B");
  auto bounded = xiaozhi::wrap_caption("A\nB\nC\nD", 120, 12, 100);
  assert(bounded[2] == "C…");
  assert(!xiaozhi::caption_line_count(xiaozhi::wrap_caption("ABC", 120, 12, 0)));
  assert(!xiaozhi::caption_line_count(xiaozhi::wrap_caption("ABC", 0, 12, 3)));
  puts("Captions: blank lines, CJK boundaries, visible ellipsis and line limits passed");
}
