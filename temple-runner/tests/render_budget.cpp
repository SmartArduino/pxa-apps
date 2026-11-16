// SPDX-License-Identifier: MIT
// Native packet/geometry audit with the real public SDK encoder. The transport
// accepts packets locally; this does not replace the simulator raster checks.
#include "../renderer.hpp"
#include <pxa/game_upload.hpp>
#include <cassert>
#include <cstdio>
#include <cmath>
namespace pxa {
Result<void> Transport::close(uint64_t) noexcept {return {};}
Result<uint32_t> Transport::io(uint64_t,uint32_t,std::span<std::byte> bytes) noexcept {return uint32_t(bytes.size());}
}
using namespace relic;
int main(){
 pxa::Transport transport;pxa::game::RenderInfo info{};
 info.capabilities=0xffffffff;info.max_draw_bytes=49152;info.max_textures=48;info.max_texture_dimension=256;
 pxa::game::Renderer target(transport,1,info);pxa::game::DrawBuffer<49152> commands;
 Renderer scene;assert(scene.upload(target));Save save;
 pxa::ui::CanvasCommands<4000> ui;pxa::ui::DisplayMetrics display;
 unsigned peak_bytes=0,peak_faces=0,frames=0;
 for(auto dimensions: {std::pair{296,240},std::pair{320,240},std::pair{240,240},std::pair{240,427}}){
  scene.width=dimensions.first;scene.height=dimensions.second;
  display.width=scene.width==240?480:scene.width;display.height=scene.height==427?854:scene.height;scene.display=&display;scene.overlay=&ui;
  for(int depth=0;depth<2;depth++)for(int quality=0;quality<2;quality++)for(unsigned seed=1;seed<=4;seed++){
   State s;start(s,seed);scene.quality=quality;scene.depth=depth;
   for(int frame=0;frame<12000;frame++){
    if(s.next_turn-s.distance<12&&s.queued_turn!=s.turn_direction)action(s,s.turn_direction<0?Action::Left:Action::Right);
    if(s.next_turn-s.distance>=30){int wanted=coin_lane(int(s.next_coin/3));if(s.lane!=wanted)action(s,wanted<s.lane?Action::Left:Action::Right);}
    for(int i=0;i<s.count;i++){auto&o=s.objects[i];float left=o.distance-s.distance;if(o.passed||left<0||left>s.speed*.3f)continue;
     if(o.kind==Kind::Gate&&s.slide<=0)action(s,Action::Slide);
     if((o.kind==Kind::Rock||o.kind==Kind::Root||o.kind==Kind::Gap)&&s.jump_t<=0)action(s,Action::Jump);
    }
    update(s,1.f/60);assert(s.mode==Mode::Running);
    if(frame%16)continue;
    scene.time=s.elapsed;ui.reset();pxa::game::Frame packet(target,commands.bytes());scene.render(packet,s,save);
    if(scene.skipped)fprintf(stderr,"Overflow: depth=%d quality=%d seed=%u distance=%g faces=%d skipped=%u\n",depth,quality,seed,s.distance,scene.count,scene.skipped);assert(scene.skipped==0);assert(packet.submit());assert(ui.bytes_used()>0);
    peak_bytes=std::max(peak_bytes,unsigned(packet.bytes_used()));peak_faces=std::max(peak_faces,unsigned(scene.count));frames++;
   }
  }
 }
 printf("Render budget passed: %u sampled frames, 4 viewports, 2 qualities, depth/fallback, 4 seeds; peak %u/%zu faces, %u/49152 packet bytes\n",frames,peak_faces,std::size(scene.faces),peak_bytes);
}
