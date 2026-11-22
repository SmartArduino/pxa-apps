// SPDX-License-Identifier: MIT
// Exercise the production drawing functions and real exported pixels.
#include "../main.cpp"
#include <cassert>
#include <climits>
#include <functional>

extern "C" int32_t pxa_call(uint32_t,uint64_t,const uint8_t*,uint32_t){return 0;}
extern "C" int32_t pxa_log_write(uint32_t,const char*){return 0;}
extern "C" int32_t pxa_io(uint64_t,uint32_t,uint8_t*,uint32_t){return 0;}

int main(){
  PixelRenderer renderer;
  for(float density:{.49f,.95f,1.f,1.35f,1.93f}){
    scale=density;ox=7;oy=5;
    renderer.resize(600,220,65536);
    std::vector<uint16_t> pixels(600*220);
    const char* item="";
    auto check=[&](std::function<void()> draw,float x,float y,float w,float h,bool left=false){
      pxa_canvas_begin(&frame,draws,sizeof(draws));
      pxa_canvas_rect_rgba(&frame,0,0,600,220,0x294d38ff,0,0,0);
      auto prefix=frame.length;draw();
      assert(renderer.paint(pixels.data(),600,frame.data,frame.length,prefix));
      int l=600,t=220,r=0,b=0;uint16_t empty=pixels[0];
      for(int yy=0;yy<220;++yy)for(int xx=0;xx<600;++xx)if(pixels[yy*600+xx]!=empty){l=std::min(l,xx);r=std::max(r,xx+1);t=std::min(t,yy);b=std::max(b,yy+1);}
      if(l>=r||t>=b){std::fprintf(stderr,"Missing pixels: %s at %.2f\n",item,density);std::abort();}
      assert(l>=px(x)&&r<=px(x+w)&&t>=py(y)&&b<=py(y+h));
      // One screen pixel of raster/filter rounding is acceptable.
      assert(std::abs((t+b)-(py(y)+py(y+h)))<=3);
      if(!left)assert(std::abs((l+r)-(px(x)+px(x+w)))<=3);
    };
    for(const auto& a:artLabels){
      // Three retired theme aliases remain in the original import manifest.
      if(!textInk(a.id))continue;
      item=a.id;
      check([&]{label(a.id,30,25,200,38);},30,25,200,38);
      check([&]{label(a.id,30,25,70,38,true);},30,25,70,38,true);
    }
    for(const auto& a:expansionLabels){
      item=a.id;
      check([&]{label(a.id,30,25,200,38);},30,25,200,38);
    }
    for(const char* value:{"0","1","8","10","99999","2147483647","-123","0/12","12/12","x1","x2"}){
      item=value;
      check([&]{numberBox(value,30,25,62,29,20);},30,25,62,29);
    }
    for(int value:{0,90,100,180,99999,INT_MAX}){
      item="price";
      check([&]{coinBox(value,30,25,90,23,17);},30,25,90,23);
    }
    item="pause";
    check([&]{pauseIcon(30,25,49,39);},30,25,49,39);
    // Tiny rounded end caps must not emit an unmatched clip-pop when their
    // destination width rounds to zero at a fractional device scale.
    for(int sample=1;sample<=320;++sample)for(bool health:{false,true}){
      pxa_canvas_begin(&frame,draws,sizeof(draws));
      pxa_canvas_rect_rgba(&frame,0,0,600,220,0x294d38ff,0,0,0);
      auto prefix=frame.length;
      progressBar(30.25f,85.5f,health?62:194,7,sample/320.f,health);
      assert(!frame.failed);
      assert(renderer.paint(pixels.data(),600,frame.data,frame.length,prefix));
    }
  }
  // The actual picker must leave the generated panel rim untouched. Exercise
  // desktop/browser scaling, portrait and narrow screens, including locked towers.
  for(auto viewport: {std::array<int,2>{800,480},{720,432},{480,854},{296,240}}){
    int width=viewport[0],height=viewport[1];
    scale=std::min(width/480.f,height/480.f);W=width/scale;H=height/scale;ox=oy=0;
    renderer.resize(width,height,65536);
    std::vector<uint16_t> panel(width*height),picker(panel.size());
    game.reset(0);game.availableTowers=3;game.coins=1000;view=Play;
    selected=0;while(!game.land(selected)||game.towers[selected].obstacle)++selected;
    for(buildPage=0;buildPage<2;++buildPage){
      pxa_canvas_begin(&frame,draws,sizeof(draws));
      pxa_canvas_rect_rgba(&frame,0,0,width,height,0x294d38ff,0,0,0);
      auto prefix=frame.length;card(12,H-83,W-24,72,0xF7ECC6);
      assert(renderer.paint(panel.data(),width,frame.data,frame.length,prefix));
      frame.length=prefix;footer();assert(!frame.failed);
      assert(renderer.paint(picker.data(),width,frame.data,frame.length,prefix));
      for(int y=py(H-83);y<py(H-11);++y)for(int x=px(12);x<px(W-12);++x)
        if(y<py(H-71)||y>=py(H-23)||x<px(24)||x>=px(W-24))
          assert(panel[y*width+x]==picker[y*width+x]);
    }
    game.availableTowers=6;
    for(int kind=1;kind<=6;++kind){
      game.towers[selected]={};game.coins=1000;buildPage=(kind-1)/3;
      auto b=pickerChoice((kind-1)%3);
      tap(b.x+b.w/2,H-80);assert(!game.towers[selected].type);
      tap(b.x+b.w+4,b.y+b.h/2);assert(!game.towers[selected].type);
      tap(b.x+b.w/2,b.y+b.h+1);assert(!game.towers[selected].type);
      tap(b.x+b.w/2,b.y+b.h/2);assert(game.towers[selected].type==kind);
    }
    game.towers[selected]={};buildPage=0;auto page=pickerPage();
    tap(page.x+page.w/2,H-81);assert(buildPage==0);
    tap(page.x+page.w/2,page.y+page.h/2);assert(buildPage==1);
    auto close=footerCloseBounds();tap(close.x+close.w/2,close.y+close.h/2);assert(selected==-1);
  }
  W=800;H=480;scale=1;ox=oy=0;
  carrotFeedback.reset(10);view=Home;
  tap(400,153);assert(carrotFeedback.tapped>0&&view==Home);
  game.reset(0);layout();view=Play;carrotFeedback.reset(game.hearts);
  int hearts=game.hearts,coins=game.coins;
  Point goal=world(game.goal());tap(goal.x,goal.y-20);
  assert(carrotFeedback.tapped>0&&game.hearts==hearts&&game.coins==coins);
  std::puts("Production lettering: all labels, long numerals and centered price groups fit at five scales");
}
