// SPDX-License-Identifier: MIT
#include "../art.hpp"
#include <cassert>
#include <vector>
#include <cstdio>
#include <cstring>
#include <string>
static void ppm(const std::string&path,const std::vector<uint16_t>&p,int w,int h){FILE*f=std::fopen(path.c_str(),"wb");assert(f);std::fprintf(f,"P6\n%d %d\n255\n",w,h);for(auto c:p){unsigned char rgb[]={uint8_t((c>>11)*255/31),uint8_t(((c>>5)&63)*255/63),uint8_t((c&31)*255/31)};std::fwrite(rgb,1,3,f);}std::fclose(f);}
int main(int argc,char**argv){
 constexpr int tools[]={0,2,4,5,6};
 for(int tool:tools){
  bool seen[8]{};
  for(int d=0;d<4;++d){auto idle=art::tool_anim::motion(d,tool,0);assert(!seen[idle.angle&7]);seen[idle.angle&7]=true;
   int duration=art::tool_anim::duration(tool);auto impact=art::tool_anim::motion(d,tool,duration/4);assert((impact.angle&7)==d*2);
  }
  for(int step=0;step<5;++step){int left=art::tool_anim::duration(tool)*(5-step)/5;
   auto east=art::tool_anim::motion(1,tool,left),west=art::tool_anim::motion(3,tool,left);
   assert(east.x==-west.x&&east.y==west.y&&((east.angle+west.angle)&7)==0);
  }
 }
 for(int id=0;id<4;++id)for(int d=0;d<4;++d){
  auto&r=art::villager_rig(id,d);auto&base=art::gait::rig(d);
  assert(r.body->w==24&&r.body->h==32&&r.side==base.side);
  for(int leg=0;leg<2;++leg)for(int row=0;row<=32;++row)assert(r.legs[leg]->rows[row]==base.legs[leg]->rows[row]);
  std::vector<uint16_t>a(48*48),b(a.size());art::Raster aa{a.data(),48,48},bb{b.data(),48,48};
  art::gait::draw(aa,24,40,d,1,false,&r);art::gait::draw(bb,24,40,d,5,false,&r);assert(a!=b);
 }
 if(argc>1){
  std::vector<uint16_t>p(800*400,art::Sky);art::Raster r{p.data(),800,400};
  for(int row=0;row<5;++row)for(int d=0;d<4;++d)for(int step=0;step<5;++step){int x=d*200+step*40+20,y=row*80+55;
   r.line(d*200,y+2,d*200+199,y+2,farm_palette::Soft);
   int remaining=step?art::tool_anim::duration(tools[row])*(5-step)/4:0;
   art::person(r,x,y,d,0,false,remaining,tools[row],false,tools[row]+1);
  }ppm(std::string(argv[1])+"/tool-poses.ppm",p,800,400);
  p.assign(768*320,art::Sky);r={p.data(),768,320};constexpr int phases[]={0,1,2,3,5,7};
  for(int id=-1;id<4;++id)for(int d=0;d<4;++d)for(int i=0;i<6;++i){int x=d*192+i*32+16,y=(id+1)*64+45;
   art::gait::draw(r,x,y,d,phases[i],false,id<0?nullptr:&art::villager_rig(id,d));
  }ppm(std::string(argv[1])+"/character-gaits.ppm",p,768,320);
 }
 puts("PASS: four distinct tool headings, mirrored east/west grips and swing axes, common NPC body/limb silhouettes and opposite contact poses");
}
