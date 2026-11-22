// SPDX-License-Identifier: MIT
#include "../pixel_renderer.hpp"
#include "pxa_canvas.h"
#include <cassert>
#include <deque>
#include <fstream>
#include <iterator>
#include <cstdio>
struct Request{uint64_t token;uint32_t offset,bytes;};
static std::deque<Request> requests;
static size_t maximum=0;
extern "C" int32_t pxa_call(uint32_t route,uint64_t token,const uint8_t* p,uint32_t){
  if(uint16_t(route)==PXA_ASSETS_SERVICE&&route>>16==PXA_ASSETS_READ){requests.push_back({token,pxa_load_u32(p+4),pxa_load_u32(p+8)});maximum=std::max(maximum,requests.size());}return 0;
}
extern "C" int32_t pxa_log_write(uint32_t,const char*){return 0;}
int main(int argc,char** argv){
  assert(argc==2);std::ifstream file(argv[1],std::ios::binary);
  std::vector<uint8_t> blob((std::istreambuf_iterator<char>(file)),{});assert(!blob.empty());
  auto a=std::lower_bound(std::begin(carrot::pixels::sources),std::end(carrot::pixels::sources),"assets/world1.png",[](const auto& s,const char* p){return std::strcmp(s.path,p)<0;});
  assert(a!=std::end(carrot::pixels::sources)&&a->external&&a->size==blob.size());
  carrot::PixelRenderer renderer;renderer.resize(800,480,65536);
  uint8_t commands[24]{};pxa_canvas_frame_t frame;pxa_canvas_begin(&frame,commands,sizeof(commands));
  assert(pxa_canvas_primitive(&frame,13,20));for(int n:{0,0,800,480})assert(pxa_put_u32(&frame,n));
  assert(pxa_put_u8(&frame,255)&&pxa_put_u8(&frame,2)&&pxa_put_u16(&frame,a-std::begin(carrot::pixels::sources)));
  assert(!renderer.stageReady(frame.data,frame.length)&&requests.size()==1);
  unsigned completed=0;
  while(!requests.empty()){
    auto request=requests.front();requests.pop_front();
    assert(request.offset+request.bytes<=blob.size());
    std::vector<uint8_t> response(12+request.bytes);pxa_store_u32(response.data()+4,request.offset);pxa_store_u32(response.data()+8,blob.size());
    std::memcpy(response.data()+12,blob.data()+request.offset,request.bytes);
    pxa_event_t e{PXA_ASSETS_SERVICE,PXA_ASSETS_READ,request.token,response.data(),uint32_t(response.size())};
    assert(renderer.event(e));++completed;
    if(renderer.loadingBackground())assert(!renderer.stageReady(frame.data,frame.length));
  }
  assert(maximum==1&&completed>4&&!renderer.loadingBackground());
  assert(renderer.stageReady(frame.data,frame.length));std::vector<uint16_t> pixels(800*480);
  assert(renderer.paint(pixels.data(),800,frame.data,frame.length,frame.length));
  assert(std::any_of(pixels.begin(),pixels.end(),[&](auto p){return p!=pixels[0];}));
  std::puts("Background staging: bounded reads and complete-frame gate passed");
}
