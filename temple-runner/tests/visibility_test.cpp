// SPDX-License-Identifier: MIT
// Compare actual public SDK draw packets with/without conservative object
// rejection. Any visible prop incorrectly culled changes the encoded image.
#include "../renderer.hpp"
#include <cassert>
#include <cstdio>
#include <cstring>
namespace pxa {
Result<void> Transport::close(uint64_t) noexcept{return {};}
Result<uint32_t> Transport::io(uint64_t,uint32_t,std::span<std::byte>b)noexcept{return uint32_t(b.size());}
}
using namespace relic;
int main(){
 pxa::Transport transport;pxa::game::RenderInfo info{};
 info.capabilities=0xffffffff;info.max_draw_bytes=49152;info.max_textures=48;info.max_texture_dimension=256;
 pxa::game::Renderer target_a(transport,1,info),target_b(transport,1,info);
 pxa::game::DrawBuffer<49152> a,b;Renderer reference,culled;
 assert(reference.upload(target_a)&&culled.upload(target_b));reference.object_culling=false;
 pxa::ui::CanvasCommands<4000> ui_a,ui_b;pxa::ui::DisplayMetrics display;Save save;
 reference.display=culled.display=&display;reference.overlay=&ui_a;culled.overlay=&ui_b;
 unsigned samples=0,rejected=0;
 for(auto size:{std::pair{296,240},std::pair{240,427},std::pair{120,213}}){
  reference.width=culled.width=size.first;reference.height=culled.height=size.second;
  display.width=size.first==296?296:480;display.height=size.second==240?240:854;
  for(int depth=0;depth<2;depth++)for(int quality=0;quality<2;quality++)for(int sample=0;sample<160;sample++){
   State s;start(s,42);s.distance=sample*7.37f;s.elapsed=4+s.distance/9;
   s.lane=sample%3-1;s.lane_x=s.lane*1.65f;s.next_turn=s.distance+80;
   if(sample%4==0){s.slide=.5f;s.pose_slide=1;}
   if(sample%4==1){s.jump_t=.45f;s.jump=1.6f;}
   if(sample%4==2){s.turns=1;s.turn_fx=sample&4?-.6f:.6f;s.turn_lean=s.turn_fx;s.next_turn=s.distance+175;}
   if(sample%4==3){s.next_turn=s.distance+9;s.queued_turn=s.turn_direction=sample&4?-1:1;s.turn_lean=s.turn_direction*.12f;}
   s.count=4;s.objects[0]={s.distance+4,-1,Kind::Rock,false};s.objects[1]={s.distance+20,2,Kind::Root,false};s.objects[2]={s.distance+37,2,Kind::Gate,false};s.objects[3]={s.distance+55,2,Kind::Gap,false};
   if(sample==0){s.count=0;s.elapsed=.5f;}
   reference.quality=culled.quality=quality;reference.depth=culled.depth=depth;reference.time=culled.time=s.elapsed;
   ui_a.reset();ui_b.reset();pxa::game::Frame fa(target_a,a.bytes()),fb(target_b,b.bytes());
   reference.render(fa,s,save);culled.render(fb,s,save);assert(fa.submit()&&fb.submit());
   if(fa.bytes_used()!=fb.bytes_used()||std::memcmp(a.bytes().data(),b.bytes().data(),fa.bytes_used())){
    fprintf(stderr,"Visibility mismatch: %dx%d depth=%d quality=%d distance=%.2f sizes=%u/%u faces=%d/%d rejected=%u\n",size.first,size.second,depth,quality,s.distance,unsigned(fa.bytes_used()),unsigned(fb.bytes_used()),reference.count,culled.count,culled.culled_objects);return 1;
   }
   assert(!reference.skipped&&!culled.skipped);rejected+=culled.culled_objects;samples++;
  }
 }
 assert(rejected>0);printf("Visibility passed: %u identical draw packets; %u whole props rejected before mesh construction\n",samples,rejected);
}
