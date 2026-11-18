// SPDX-License-Identifier: MIT
// Exercise the production UI and actor compositor with real native font metrics.
#define main starsoil_layout_original_main
#include "layout_test.cpp"
#undef main
static void contained(const pocket::TextMark &t,int x,int y,int w,int h,int pad) {
  if(t.x<x+pad || t.y<y+pad || t.x+t.w>x+w-pad || t.y+t.h>y+h-pad) {
    std::fprintf(stderr,"Text outside frame: %s at %d,%d %dx%d; frame %d,%d %dx%d pad%d\n",t.value.c_str(),t.x,t.y,t.w,t.h,x,y,w,h,pad);
    assert(false);
  }
}
int main() {
  game::init();pocket::controller_mode=true;session_active=true;
  for(auto [lw,lh,pw,ph]:{std::tuple{288,288,480,480},std::tuple{299,224,480,360},
      std::tuple{336,224,480,320},std::tuple{288,384,360,480},
      std::tuple{288,288,320,320},std::tuple{288,288,240,240}}) {
    pocket::WIDTH=lw;pocket::HEIGHT=lh;pocket::buffer.resize(lw*lh);pocket::painted.resize(lw*lh);
    pocket::screen.resize(pw*ph);pocket::ui={pocket::screen.data(),pw,ph,lw,lh};pocket::screen_mode=true;
    for(bool en:{false,true}) {
      pocket::set_english(en);new_game(s,1234567);s.equip[0]=CrystalHelmet;s.equip[1]=CrystalMail;s.equip[2]=CrystalGreaves;s.equip[7]=GlowCharm;
      for(auto page:{Bag,Crafting,Gear}) {
        set_view(page);
        if(page==Crafting)post_notice(tr("Free some bag space for the result.","背包空间不足，请先腾出成品位置。"));
        pocket::text_marks.clear();pocket::trace_text=true;game::draw();pocket::trace_text=false;
        int cx=content_x(),cy=content_y(),fy=height()-224-FooterInset;
        for(auto &t:pocket::text_marks) {
          if(t.y<28 && t.x<width()-64) {
            if(page==Crafting && t.x>=width()-162)contained(t,width()-162,5,98,23,2);
            else contained(t,6,5,width()-12,23,2);
          }
          if(t.y>=28 && t.y<48) {
            int group=(t.x-5)*5/(width()-8),x=5+group*(width()-8)/5;
            contained(t,x,28,(width()-8)/5-3,20,2);
          }
          if(page==Crafting && t.y>=49+cy && t.y<49+cy+recipes_per_page()*RecipeRowHeight) {
            int row=(t.y-cy-49)/RecipeRowHeight;
            contained(t,6+cx,49+cy+row*RecipeRowHeight,276,RecipeRowHeight-2,3);
          }
          if(page==Gear && t.x>=129+cx && t.y>=49+cy && t.y<49+cy+7*gear_row_height()) {
            int row=(t.y-cy-49)/gear_row_height();contained(t,129+cx,49+cy+row*gear_row_height(),151,gear_row_height()-1,1);
          }
          if(page==Bag && t.x>=210+cx && t.y>=48+cy && t.y<172+cy)contained(t,210+cx,48+cy,72,124,4);
          int by=(page==Bag?204:page==Gear?203:199)+fy;
          if(t.y==by+(20-pocket::text_height())/2) {
            if(page==Bag)contained(t,(t.x<140+cx?6:143)+cx,by,t.x<140+cx?128:139,20,2);
            if(page==Gear)contained(t,8+cx,by,272,20,2);
            if(page==Crafting)for(auto [x,w]:{std::pair{6,43},std::pair{111,76},std::pair{193,41},std::pair{239,43}})
              if(t.x>=x+cx && t.x<x+cx+w)contained(t,x+cx,by,w,20,2);
          }
        }
      }
    }
  }
  // Every held item, both directions: compose actual visible pixels and ensure
  // the paper-doll pose and weapon fit within all preview frames.
  for(auto [x,y,w,h]:{std::tuple{210,52,72,78},std::tuple{8,49,113,98},std::tuple{8,49,113,126}})
    for(int held=0;held<ItemCount;++held)for(bool face:{false,true}) {
      auto q=preview_placement(x,y,w,h,s.equip,held,face);
      starsoil_actor::Appearance a{q.x,q.feet,art::StandFrame,q.scale,255,0,face,false,s.equip,uint8_t(held)};
      starsoil_actor::draw(a,[&](int px,int py,uint16_t,starsoil_actor::Tag,uint8_t) {
        assert(px>=x+4 && px<x+w-4 && py>=y+4 && py<y+h-4);
      });
    }
  puts("PASS native Chinese/English text stays inside headers, tabs, recipe/equipment cards, previews and buttons at six output geometries; all held-item previews fit with padding");
}
