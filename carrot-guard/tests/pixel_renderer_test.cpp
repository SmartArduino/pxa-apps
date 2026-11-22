// SPDX-License-Identifier: MIT
#include "../pixel_renderer.hpp"
#include "pxa_canvas.h"
#include <cassert>
#include <cstdio>

static unsigned assetReads=0;
extern "C" int32_t pxa_call(uint32_t route,uint64_t,const uint8_t*,uint32_t){if(uint16_t(route)==PXA_ASSETS_SERVICE&&route>>16==PXA_ASSETS_READ)++assetReads;return 0;}
extern "C" int32_t pxa_log_write(uint32_t,const char*){return 0;}
int main(int argc,char** argv){
  carrot::PixelRenderer renderer;renderer.resize(200,120,65536);
  std::vector<uint16_t> first(200*120),second(200*120),reference;
  uint8_t commands[4096];pxa_canvas_frame_t frame;
  auto begin=[&](){pxa_canvas_begin(&frame,commands,sizeof(commands));assert(pxa_canvas_rect_rgba(&frame,0,0,200,120,0x285834ff,0,0,0));return frame.length;};
  auto draw=[&](const char* path,int x,int y,int w,int h){assert(pxa_canvas_image(&frame,x,y,w,h,255,1,path,std::strlen(path)));};
  size_t prefix=begin();draw("assets/h1-00.png",20,10,86,86);
  assert(renderer.paint(first.data(),200,frame.data,frame.length,prefix));reference=first;
  assert(renderer.paint(second.data(),200,frame.data,frame.length,prefix));assert(first==second&&renderer.hits);
  // Composing two clips must exactly reproduce a single image, even when
  // their boundary crosses transparent antialiased contours.
  prefix=begin();assert(pxa_canvas_clip_push(&frame,0,0,63,120));draw("assets/h1-00.png",20,10,86,86);assert(pxa_canvas_clip_pop(&frame));
  assert(pxa_canvas_clip_push(&frame,63,0,137,120));draw("assets/h1-00.png",20,10,86,86);assert(pxa_canvas_clip_pop(&frame));
  assert(renderer.paint(second.data(),200,frame.data,frame.length,prefix));assert(reference==second);
  // Movement/disappearance restores the previous position in both buffers.
  prefix=begin();draw("assets/h1-00.png",110,10,86,86);assert(renderer.paint(first.data(),200,frame.data,frame.length,prefix));
  prefix=begin();assert(renderer.paint(second.data(),200,frame.data,frame.length,prefix));
  assert(std::all_of(second.begin(),second.end(),[&](auto p){return p==second[0];}));
  // Force both arenas through eviction/compaction using all generated actors
  // and labels; a previously cached image must still render identically.
  for(const auto& a:carrot::pixels::sources)if(!a.external){prefix=begin();draw(a.path,-7,-3,173,113);assert(renderer.paint(second.data(),200,frame.data,frame.length,prefix));}
  prefix=begin();draw("assets/h1-00.png",20,10,86,86);assert(renderer.paint(second.data(),200,frame.data,frame.length,prefix));assert(reference==second);
  // A region selects one complete generated frame without rounding/scaling
  // the rest of its atlas. At 1:1 it equals clipping the original atlas.
  prefix=begin();pxa_canvas_clip_push(&frame,20,10,43,43);draw("assets/h1-00.png",-23,-76,172,172);pxa_canvas_clip_pop(&frame);
  assert(renderer.paint(first.data(),200,frame.data,frame.length,prefix));
  prefix=begin();const char* path="assets/h1-00.png";
  assert(pxa_canvas_primitive(&frame,12,26+std::strlen(path)));
  for(int value:{20,10,43,43})assert(pxa_put_u32(&frame,value));
  assert(pxa_put_u8(&frame,255)&&pxa_put_u8(&frame,1));
  for(int value:{43,86,43,43})assert(pxa_put_u16(&frame,value));
  assert(pxa_put_bytes(&frame,reinterpret_cast<const uint8_t*>(path),std::strlen(path)));
  assert(renderer.paint(second.data(),200,frame.data,frame.length,prefix));assert(first==second);
  frame.data[prefix+1]=2;
  assert(renderer.paint(second.data(),200,frame.data,frame.length,prefix));
  for(int y=0;y<43;++y)for(int x=0;x<43;++x)assert(first[(10+y)*200+20+x]==second[(10+y)*200+20+42-x]);
  auto at=std::lower_bound(std::begin(carrot::pixels::sources),std::end(carrot::pixels::sources),path,[](const auto& a,const char* b){return std::strcmp(a.path,b)<0;});
  unsigned id=at-std::begin(carrot::pixels::sources);assert(!std::strcmp(at->path,path));
  prefix=begin();assert(pxa_canvas_primitive(&frame,14,28));
  for(int value:{20,10,43,43})assert(pxa_put_u32(&frame,value));
  assert(pxa_put_u8(&frame,255)&&pxa_put_u8(&frame,1));
  for(int value:{43,86,43,43,int(id)})assert(pxa_put_u16(&frame,value));
  assert(renderer.paint(second.data(),200,frame.data,frame.length,prefix));assert(first==second);
  // Cached HUD corners remain transparent over changing units underneath.
  // RGB565 premultiplication may round a mixed channel by one quantization step.
  for(int phase=0;phase<2;++phase){
    prefix=begin();pxa_canvas_rect_rgba(&frame,10+phase*18,5,80,35,0xb04860ff,0,0,0);
    size_t hudBegin=frame.length;
    pxa_canvas_rect_rgba(&frame,20,10,100,30,0x80a040a0,12,0,0);
    pxa_canvas_rect_rgba(&frame,15,95,150,20,0xffdf9aff,7,0,0);
    size_t hudEnd=frame.length;
    assert(renderer.paint(first.data(),200,frame.data,frame.length,prefix));
    assert(renderer.paint(second.data(),200,frame.data,frame.length,prefix,hudBegin,hudEnd,77,45,90));
    for(size_t i=0;i<first.size();++i)for(auto mask: {0xf800,0x7e0,0x1f}){
      int shift=mask==0xf800?11:mask==0x7e0?5:0;
      assert(std::abs(int((first[i]&mask)>>shift)-int((second[i]&mask)>>shift))<=1);
    }
  }
  // Changing either HUD band must preserve the other band and clear old
  // translucent pixels, including when numbers shrink or disappear.
  carrot::PixelRenderer unsplit;unsplit.resize(200,120,65536);
  for(unsigned phase=0;phase<18;++phase){
    prefix=begin();pxa_canvas_rect_rgba(&frame,0,0,200,120,0x345678ff+phase*0x1000000,0,0,0);
    size_t hudBegin=frame.length;
    pxa_canvas_rect_rgba(&frame,8,4,180,30,0xffdf9aff,8,0,0);
    if(phase%3)pxa_canvas_rect_rgba(&frame,20,8,30+phase%4*20,20,0x80a040a0,4,0,0);
    size_t split=frame.length-hudBegin;
    pxa_canvas_rect_rgba(&frame,15,95,150,20,0xffdf9aff,7,0,0);
    if(phase/6%2)draw("assets/h1-00.png",110,92,25,25);
    size_t hudEnd=frame.length;
    assert(unsplit.paint(first.data(),200,frame.data,frame.length,prefix,hudBegin,hudEnd,100+phase,40,90));
    assert(renderer.paint(second.data(),200,frame.data,frame.length,prefix,hudBegin,hudEnd,100+phase,40,90,split));
    assert(first==second);
  }
  // Retained Surface buffers restore their own history, including disappearing
  // sprites/HUD, backdrop changes, clips and scanline padding. Compare against
  // an independent full restoration rather than assuming alternation.
  for(int stride:{200,208}){
    carrot::PixelRenderer retained,full;
    retained.resize(200,120,65536);full.resize(200,120,65536);
    std::array<std::vector<uint16_t>,2> buffers{std::vector<uint16_t>(stride*120,0x1234),std::vector<uint16_t>(stride*120,0x1234)};
    std::vector<uint16_t> expected(stride*120,0x1234);
    for(unsigned phase=0;phase<90;++phase){
      pxa_canvas_begin(&frame,commands,sizeof(commands));
      pxa_canvas_rect_rgba(&frame,0,0,200,120,phase<45?0x285834ff:0x543828ff,0,0,0);prefix=frame.length;
      if(phase%4){pxa_canvas_clip_push(&frame,8,3,180,112);draw("assets/h1-00.png",int(phase*11%180)-20,int(phase*17%110)-12,43,43);pxa_canvas_clip_pop(&frame);}
      size_t hudBegin=frame.length;
      if(phase%7)pxa_canvas_rect_rgba(&frame,8,4,180,30,0xffdf9ac0,8,0,0);
      size_t split=frame.length-hudBegin;
      pxa_canvas_rect_rgba(&frame,15,95,150,20,0xffdf9aff,7,0,0);
      size_t hudEnd=frame.length;
      unsigned index=phase%3?0:1;
      assert(full.paint(expected.data(),stride,frame.data,frame.length,prefix,hudBegin,hudEnd,phase+1,40,90,split));
      assert(retained.paint(buffers[index].data(),stride,frame.data,frame.length,prefix,hudBegin,hudEnd,phase+1,40,90,split,index));
      assert(buffers[index]==expected);
      if(phase==30)retained.invalidate();
    }
  }
  renderer.resize(200,120,88474);prefix=begin();draw("assets/ui/s5.png",0,0,80,80);assert(renderer.paint(second.data(),200,frame.data,frame.length,prefix));
  if(argc>1){
    renderer.resize(800,480,65536);std::vector<uint16_t> pixels(800*480);
    pxa_canvas_begin(&frame,commands,sizeof(commands));draw("assets/world0.png",0,0,800,480);frame.data[21]=2;
    assert(renderer.paint(pixels.data(),800,frame.data,frame.length,frame.length));
    FILE* output=std::fopen(argv[1],"wb");assert(output);assert(std::fwrite(pixels.data(),2,pixels.size(),output)==pixels.size());std::fclose(output);
  }
  assert(assetReads==0); // The title background never waits on asynchronous reads.
  std::puts("Surface pixels: clips, alpha, two-buffer restoration, cache eviction and density passed");
}
