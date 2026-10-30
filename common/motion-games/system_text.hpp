// SPDX-License-Identifier: MIT
#pragma once
#include <pxa/ui.hpp>
#include <pxa/ui_display.hpp>
#include <pxa_canvas.h>
#include <array>
#include <string_view>

namespace motion {
// Transparent, retained Canvas above the GameRender Surface. Glyphs are
// resolved and cached by the Host's system font at the actual native size.
struct SystemText {
 std::array<uint8_t,8192> bytes{};std::array<uint8_t,512> packet{};
 pxa_canvas_frame_t frame{};uint32_t generation=0,last_hash=0,density=65536;
 uint32_t node_id=0;uint8_t initialized=1;int scale=1,width=240,height=284;
 bool create(pxa::Transport& transport,const pxa::ui::DisplayMetrics& display,int render_scale){
  density=display.density_q16?display.density_q16:65536;scale=render_scale;width=int(display.width)/scale;height=int(display.height)/scale;
  (void)transport;return node_id!=0;
 }
 struct View {
  SystemText& owner;static constexpr pxa::ui::Capacity capacity{1,0,0,0,0};static constexpr bool overlay=true;
  template<class Page> bool render(Page& page,uint32_t parent){using namespace pxa::ui;auto node=page.create(parent,protocol::canvas);if(!node)return false;owner.node_id=node;auto& tx=page.transaction();return tx.fill(node,protocol::width)&&tx.fill(node,protocol::height)&&tx.rgba(node,protocol::background,0)&&tx.u8(node,protocol::composition,protocol::alpha_overlay)&&tx.u64(node,protocol::event_mask,0);}
 };
 int logical(int native)const{return int((int64_t(native)*65536+int(density/2))/density);}
 void begin(){pxa_canvas_begin(&frame,bytes.data(),bytes.size());}
 void text(int x,int y,int w,int h,std::string_view value,uint16_t color,int align,int pixels){
  if(value.empty()||w<=0||h<=0)return;
  unsigned r=((color>>11)&31)*255/31,g=((color>>5)&63)*255/63,b=(color&31)*255/31;uint32_t rgba=(r<<24)|(g<<16)|(b<<8)|255;
  int font=std::clamp(pixels*scale,8,128),top=y*scale,box_height=h*scale,py=top+(box_height-font)/2-font/4;
  int xx=logical(x*scale),yy=logical(top),ww=std::max(1,logical(w*scale)),hh=std::max(1,logical(box_height));
  if(!pxa_canvas_clip_push(&frame,xx,yy,ww,hh))return;
  // Public UI SIZED_TEXT command 12: font size remains native pixels, while
  // Canvas geometry follows the display density rather than the game scale.
  if(pxa_canvas_primitive(&frame,12,uint16_t(24+value.size())))
   (void)(pxa_put_u32(&frame,uint32_t(xx))&&pxa_put_u32(&frame,uint32_t(logical(py)))&&pxa_put_u32(&frame,uint32_t(ww))&&pxa_put_u32(&frame,uint32_t(std::max(1,logical(font*2+4))))&&pxa_put_u32(&frame,rgba)&&pxa_put_u16(&frame,uint16_t(font))&&pxa_put_u8(&frame,uint8_t(align))&&pxa_put_u8(&frame,0)&&pxa_put_bytes(&frame,reinterpret_cast<const uint8_t*>(value.data()),value.size()));
  (void)pxa_canvas_clip_pop(&frame);
 }
 bool present(){
  uint32_t hash=2166136261u;for(size_t i=0;i<frame.length;++i)hash=(hash^bytes[i])*16777619u;
  if(hash==last_hash)return true;
  if(!pxa_canvas_present_regions(node_id,&frame,&generation,&initialized,0,nullptr,0,packet.data(),packet.size(),nullptr,0))return false;
  last_hash=hash;return true;
 }
};
}
