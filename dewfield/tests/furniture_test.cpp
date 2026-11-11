// SPDX-License-Identifier: MIT
#include "../scene.hpp"
#include <cassert>
#include <cstdio>
#include <vector>
using namespace farm_scene;
constexpr int W=320,H=160,X=160,Y=120;
struct Canvas {art::Raster&r;void rect(int x,int y,int w,int h,uint16_t c){r.rect(x,y,w,h,c);}};
int main(){
 std::vector<uint16_t> a(W*H),b(W*H);
 // One packed bank serves native scenes and the Canvas-based forecast icon.
 // Also verify odd span offsets, palette variants and real sole contacts.
 for(auto*image:furniture_sprites::All){
  const auto&s=image->shape;assert(s.count&&s.shadow&&s.anchor_y==s.h);
  for(unsigned i=0;i<s.count;++i){const auto&p=s.spans[i];assert(p.x+p.count<=s.w&&p.y<s.h);}
  int contacts=0;
  for(unsigned i=0;i<s.shadow->count;++i){const auto&p=s.shadow->spans[i];
   for(int x=p.x;x<p.x+p.count;++x){assert(!art::hit(s,x,p.y));if(p.alpha>1){assert(art::hit(s,x,p.y-1));++contacts;}}
  }
  assert(contacts>0);
  for(int style=0;style<image->palettes;++style){
   std::fill(a.begin(),a.end(),0);std::fill(b.begin(),b.end(),0);
   art::Raster r{a.data(),W,H};art::furniture(r,*image,X,Y,style);
   art::Raster rr{b.data(),W,H};Canvas c{rr};art::furniture(c,*image,X,Y,style);assert(a==b);
  }
 }
 farm::State state;farm::fresh(state);state.region=unsigned(farm::Region::ElderHome);
 // Every visible pixel, including tools/flowers/flames, stays inside the
 // scene rectangle used to cull and depth-sort the corresponding furniture.
 for(auto kind:{Kind::Bed,Kind::Table,Kind::Chair,Kind::Cabinet,Kind::Stove,Kind::Fireplace,Kind::Workbench,Kind::Bookcase,Kind::WoodRack,Kind::Divider,Kind::Armchair,Kind::Television,Kind::Calendar,Kind::Bench,Kind::Planter,Kind::Counter,Kind::Shelf,Kind::KitchenCounter}){
  Queue q;q.viewport={0,0,W,H};q.add(kind,kind==Kind::Counter?60:X,Y,Y,3);assert(q.count==1);
  std::fill(a.begin(),a.end(),0);art::Raster r{a.data(),W,H};q.objects(r,state,{});auto box=bounds(q.entries[0]);int painted=0;
  for(int y=0;y<H;++y)for(int x=0;x<W;++x)if(a[y*W+x]){assert(box.intersects({x,y,1,1}));++painted;assert(y<Y);}
  assert(painted>100);
 }
 // Correct side/back views are selected independently of palette/material.
 for(auto kind:{Kind::Chair,Kind::Armchair,Kind::Cabinet}){
  Entry e{kind,160,120,120,0,0,false};const furniture_sprites::Image*views[4]{};
  for(int d=0;d<4;++d){e.facing=d;views[d]=furniture_image(e);}
  assert(views[0]!=views[2]&&views[1]!=views[3]&&views[1]!=views[2]);
 }
 for(auto origin:{farm::Point{-70,-50},farm::Point{150,110},farm::Point{400,300}}){
  std::vector<uint16_t> frame(23*19+32,0xabcd);art::Raster r{frame.data()+16,23,19,origin.x,origin.y};
  for(auto*image:furniture_sprites::All)art::furniture(r,*image,X,Y);
  for(int i=0;i<16;++i)assert(frame[i]==0xabcd&&frame[16+23*19+i]==0xabcd);
 }
 puts("PASS: furniture palettes, packed native/UI agreement, true sole contacts, complete culling bounds and clipped canvases");
}
