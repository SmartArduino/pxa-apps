// SPDX-License-Identifier: MIT
#include "../font_codec.hpp"
#include <font.hpp>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <vector>
#include <cstdio>
int main(int argc,char** argv){assert(argc==2);using namespace motion;size_t total=0;
 for(unsigned i=0;i<font_pages;++i){size_t size=size_t(font_page_width)*font_page_heights[i];std::vector<std::byte> pixels(size);assert(unpack_font(font_packed_pages[i],pixels,font_page_width,font_page_filters[i]));
  std::vector<std::byte> original(size);auto path=std::filesystem::path(argv[1])/"resources"/("font-"+std::to_string(i)+".index8");std::ifstream file(path,std::ios::binary);assert(file.read(reinterpret_cast<char*>(original.data()),std::streamsize(size)));assert(file.peek()==EOF);assert(pixels==original);total+=size;
  if(font_packed_pages[i].size()>3)assert(!unpack_font(font_packed_pages[i].first(1),pixels,font_page_width,font_page_filters[i]));}
 std::array<std::byte,3> out{};std::array<uint8_t,3> invalid{128,0,0};assert(!unpack_font(invalid,out,1,0));std::array<uint8_t,1> truncated{127};assert(!unpack_font(truncated,out,1,0));std::array<uint8_t,4> literal{2,1,2,3};assert(unpack_font(literal,out,1,2));assert(out[1]==std::byte{2}&&out[2]==std::byte{3});assert(!unpack_font(literal,out,1,5));
 std::printf("PASS: %u font pages, %zu coverage bytes identical; truncated literals, invalid back-references and filters rejected\n",font_pages,total);
}
