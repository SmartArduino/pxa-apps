// SPDX-License-Identifier: MIT
#include "../text_pages.hpp"
#include <cassert>
#include <iostream>
int main() {
 const std::vector<std::string> samples={"", "第一行\n第二行\n第三行", "\n\n\n\n", "ABCDEFGHIJKLMNOPQRSTUVWXYZ WWWW MMMM words remain complete", "罍殽 café 拼音 👩‍🚀\n最后一行。",std::string(1200,'W')};
 for(const auto &text:samples)for(unsigned cols:{2,3,12,20,70,160})for(unsigned rows:{1,2,4,8,24,64}) {
  auto pages=sibo::text_pages(text,cols,rows);assert(!pages.empty());std::string joined;
  for(const auto &p:pages) {
   assert(p.empty() || (static_cast<unsigned char>(p.front())&0xc0)!=0x80);
   assert(!p.empty() || text.empty());joined+=p;
  }
  assert(joined==text);
 }
 assert(sibo::text_pages("第一行\n第二行",8,1).size()==2);
 assert(sibo::text_pages(std::string(1200,'W'),70,24).size()<sibo::text_pages(std::string(1200,'W'),70,8).size());
 std::cout<<"Tiny one-line/tablet multi-line pagination, UTF-8 boundaries and byte conservation PASS\n";
}
