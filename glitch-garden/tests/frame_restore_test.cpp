// SPDX-License-Identifier: MIT
#include "../scene.hpp"
#include "dense_scene.hpp"
#include <cassert>
#include <iostream>
using namespace garden;
int main(){
 FilteredArt art;art.bytes.resize(garden_art::pixel_bytes);assert(unpack(garden_art::packed_pixels,art.bytes));art.cache_budget=1536*1024;
 for(int width:{480,320}){int height=width,stride=width+7;float zoom=float(width)/296;Layout layout;layout.w=296;layout.h=296;layout.cell=25;layout.bx=38;layout.by=93;
  FrameRestore restore;std::array<std::vector<uint16_t>,3> buffers;for(auto& b:buffers)b.assign(stride*height,0xdead);std::vector<uint16_t> background(width*height),expected(stride*height,0xdead);Game game;game.start(0);uint64_t revision=1;
  for(int frame=0;frame<240;++frame){unsigned index=unsigned(frame/2+frame%3)%3;art.cached_motion=frame%17<12;
   if(frame%23==0){++revision;for(unsigned i=0;i<background.size();++i)background[i]=uint16_t((i*17+frame*997)%65536);}
   for(int y=0;y<height;++y)std::copy_n(background.data()+y*width,width,expected.data()+y*stride);
   restore.restore(index,buffers[index].data(),stride,background.data(),width,height,revision);
   DamageBounds damage;damage.include(board_damage(layout,zoom,width,height));Paint incremental{buffers[index].data(),width,height,stride,art,zoom},full{expected.data(),width,height,stride,art,zoom};
   dense_scene(game,frame*.11f,frame%61<40);for(auto& e:game.enemies)if(e.active)e.x-=float(frame%45)*.21f;
   auto draw=[&](Paint& p){if(frame%41<30)draw_board(p,game,layout);if(frame%29<5){p.image(garden_art::ui_pause_panel,{30,20,225,224});p.number({90,40,100,33},frame);}if(frame%13<4){p.image(garden_art::ui_notice,{-8,140,122,30});p.number({0,145,90,23},99-frame);}p.arrow({270,250,27,27},frame%2,true);p.rect(0,frame%280,5,3,0xffff);};
   if(frame%29<5)damage.include(incremental.physical({30,20,225,224}));if(frame%13<4)damage.include(incremental.physical({-8,140,122,30}));damage.include(incremental.physical({270,250,27,27}));damage.include(incremental.physical({0,frame%280,5,3}));
   draw(incremental);draw(full);restore.finish(index,damage.rect);
   if(buffers[index]!=expected){std::cerr<<"Mismatch frame="<<frame<<" width="<<width<<"\n";return 1;}
  }
 }
 // Unknown buffer indices must safely fall back to restoring the whole frame.
 FrameRestore restore;uint16_t bg[]={1,2,3,4},dest[]={9,9,9,9};restore.restore(5,dest,2,bg,2,2,1);assert(std::equal(std::begin(bg),std::end(bg),std::begin(dest)));
 std::cout<<"Incremental frames equal full restoration across buffer reuse, clips, notices, modals and background changes\n";
}
