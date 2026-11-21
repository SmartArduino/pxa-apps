// SPDX-License-Identifier: MIT
#include "art.hpp"
#include "story.hpp"
#include "story_text.hpp"
#include <cassert>
#include <cstdio>
#include <string>
#include <vector>
using namespace iron;
int main(){unsigned frames=0,pages=0;
 for(int chapter:{0,3,4,5,lighthouse_chapter})for(float zoom:{.72f,1.f,1.3f})for(Vec camera:{Vec{8,10},Vec{23,21},Vec{38,36}}){World w;w.init(chapter);w.fog.fill(1);w.seen.fill(1);std::vector<uint16_t> a(720*420,navy),b=a;Raster ra(a.data(),720,420,720),rb(b.data(),720,420,720);Battlefield x,y;x.width=y.width=720;x.top=y.top=0;x.bottom=y.bottom=420;x.zoom=y.zoom=zoom;x.cache_ground=y.cache_ground=false;x.focus(camera);y.focus(camera);y.cull_tiles=false;x.draw(ra,w);y.draw(rb,w);assert(a==b);++frames;}
 for(auto size:{Pt{296,240},Pt{800,480},Pt{854,480},Pt{1920,1080}}){int width=int(size.x),height=int(size.y),cw=width==296?480:width==800?533:width,ch=width==296?389:width==800?320:height;NativeText native;native.reset(width==800?400:width,width==800?240:height,cw,ch);std::vector<uint16_t> p(cw*ch);Raster r(p.data(),cw,ch,cw);r.native_text=&native;
  for(auto& chapter:chapters){Rect area{width==296?24:94,width==296?130:104,cw-(width==296?48:118),ch-143-(width==296?130:104)-8};StoryText text(r,chapter.briefing,area.w);std::string joined,original;for(auto paragraph:chapter.briefing)original+=paragraph;for(unsigned i=0;i<text.count;++i){assert(r.ink_width(text.lines[i],12)<=area.w);joined+=text.lines[i];}assert(joined==original);for(unsigned page=0;page<text.pages(r,area);++page){native.reset(width==800?400:width,width==800?240:height,cw,ch);text.draw(r,area,page);assert(!native.overflow);++pages;}}
  for(auto& radio:radio_scripts){Rect area{38,121,cw-76,ch-194};StoryText text(r,radio.lines,area.w);std::string joined,original;for(auto line:radio.lines)original+=line;for(unsigned i=0;i<text.count;++i){assert(r.ink_width(text.lines[i],12)<=area.w);joined+=text.lines[i];}assert(joined==original);}
 }
 {NativeText queue;queue.reset(296,240,480,389);std::vector<uint16_t> p(480*389);Raster r(p.data(),480,389,480);r.native_text=&queue;Rect narrow{24,100,130,48};StoryText text(r,chapters[5].briefing,narrow.w);assert(text.pages(r,narrow)>1);std::string visible,original;for(auto line:chapters[5].briefing)original+=line;for(unsigned page=0;page<text.pages(r,narrow);++page){queue.reset(296,240,480,389);text.draw(r,narrow,page);assert(!queue.overflow);for(unsigned i=0;i<queue.count;++i){auto c=queue.commands[i];visible.append(queue.strings.data()+c.offset,c.length);}}assert(visible==original);}
 for(unsigned i=0;i<20;++i){auto& f=art::frames[art::construction_first+i];assert(f.by+f.bh-f.oy==13&&f.count>0);auto& profile=art::shadow_profiles[art::construction_first+i];assert(profile.count&&profile.offset+profile.count<=art::shadow_columns.size());}
 std::printf("%u full/cull battlefield comparisons, %u lossless briefing pages, %zu radio scripts and 20 registered construction frames passed\n",frames,pages,std::size(radio_scripts));
}
