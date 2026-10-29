// SPDX-License-Identifier: MIT
#pragma once
#include "font_codec.hpp"
#include <pxa/game_upload.hpp>
namespace motion {
template<class App> pxa::Result<void> upload_embedded_fonts(App& app){
 auto scratch=app.buffer.bytes();
 for(unsigned i=0;i<font_pages;++i){size_t pixels=size_t(font_page_width)*font_page_heights[i];
  if(pixels+pxa::game::Upload::header_bytes>scratch.size())return std::unexpected(pxa::Error::limit_exceeded);
  auto decoded=scratch.subspan(pxa::game::Upload::header_bytes,pixels);
  if(!unpack_font(font_packed_pages[i],decoded,font_page_width,font_page_filters[i]))return std::unexpected(pxa::Error::protocol_error);
  auto sent=pxa::game::Upload(*app.renderer,scratch).texture({uint8_t(i)},font_page_width,font_page_heights[i],decoded);if(!sent)return sent;
 }
 return {};
}
}
