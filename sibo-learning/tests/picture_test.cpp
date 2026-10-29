// SPDX-License-Identifier: MIT
#include "../picture_view.hpp"
#include <cassert>
#include <iostream>
using namespace sibo;
static void bounded(const PictureView &v) {
  auto r=v.rectangle();assert(r.width>0&&r.height>0);
  if(r.width>v.width)assert(r.x<=0&&r.x+r.width>=v.width);
  else assert(v.pan_x==0&&r.x>=0&&r.x+r.width<=v.width);
  if(r.height>v.height)assert(r.y<=0&&r.y+r.height>=v.height);
  else assert(v.pan_y==0&&r.y>=0&&r.y+r.height<=v.height);
}
int main() {
  PictureView v;v.initialize(100,100,200,200);auto fit=v.rectangle();
  assert(fit.x==0&&fit.y==0&&fit.width==200&&fit.height==200);
  v.zoom(1,200,200,50,50);auto anchored=v.rectangle();
  assert(anchored.x==-50&&anchored.y==-50&&anchored.width==400&&anchored.height==400);
  for(auto iw:{1,127,256,1024})for(auto ih:{1,181,512,1024})for(auto w:{48,218,480,1024})for(auto h:{48,129,320,600}) {
    v.initialize(iw,ih,w,h);fit=v.rectangle();bounded(v);
    for(int level=1;level<6;++level) {
      v.zoom(level,w,h,w/3,h/3);bounded(v);
      v.move(100000,100000);bounded(v);v.move(-200000,-200000);bounded(v);
      v.zoom(level,w+31,h+17);bounded(v);v.zoom(level,w,h);bounded(v);
    }
    v.zoom(0,w,h);auto reset=v.rectangle();
    assert(fit.x==reset.x&&fit.y==reset.y&&fit.width==reset.width&&fit.height==reset.height);
  }
  std::cout<<"Picture viewport: fit, 1–8x zoom, anchor, pan edges and reset passed\n";
}
