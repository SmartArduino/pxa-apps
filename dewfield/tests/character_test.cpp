// SPDX-License-Identifier: MIT
#include "../art.hpp"
#include <cassert>
#include <vector>
#include <cstdio>
#include <string>
static void ppm(const std::string&path,const std::vector<uint16_t>&p,int w,int h,int zoom=1) {
 FILE*f=std::fopen(path.c_str(),"wb");assert(f);std::fprintf(f,"P6\n%d %d\n255\n",w*zoom,h*zoom);
 for(int y=0;y<h;++y)for(int dy=0;dy<zoom;++dy)for(int x=0;x<w;++x){auto c=p[y*w+x];unsigned char rgb[]={uint8_t((c>>11)*255/31),uint8_t(((c>>5)&63)*255/63),uint8_t((c&31)*255/31)};for(int dx=0;dx<zoom;++dx)std::fwrite(rgb,1,3,f);}std::fclose(f);
}
int main(int argc,char**argv) {
 constexpr int tools[]={0,2,4,5,6};
 for(int tool:tools)for(int grade=0;grade<3;++grade)for(int angle=0;angle<16;++angle) {
  const auto &a=art::tool_anim::held_frame(tool,grade,angle);
  const auto &b=art::tool_anim::held_frame(tool,grade,(-angle)&15,true);
  std::vector<uint16_t>pa(44*44),pb(pa.size());art::Raster ra{pa.data(),44,44},rb{pb.data(),44,44};
  art::grounded(ra,*a.sprite,22,22);art::grounded(rb,*b.sprite,22,22);
  for(int y=0;y<44;++y)for(int x=1;x<44;++x)assert(pa[y*44+x]==pb[y*44+44-x]);
  bool blade=false;for(int dy=-1;dy<=1;++dy)for(int dx=-1;dx<=1;++dx)blade|=art::hit(*a.sprite,22+a.tip_x+dx,22+a.tip_y+dy);
  assert(blade&&art::hit(*a.sprite,22,22));
 }
 for(int tool:{0,4,5})for(int grade=0;grade<3;++grade) {
  int duration=art::tool_anim::duration(tool,grade),remaining=art::tool_anim::impact_remaining(tool,grade);
  for(int d=0;d<4;++d) {
   int dx=(d==1)-(d==3),dy=(d==2)-(d==0);
   int tx=dx*16,ty=dy*16-art::tool_anim::surface_offset(tool);
   auto m=art::tool_anim::motion(d,tool,remaining,grade,tx,ty,true);
   const auto&head=art::tool_anim::held_frame(tool,grade,m.angle,m.mirrored);
   assert(m.progress>=art::tool_anim::Impact&&m.x+head.tip_x==tx&&m.y+head.tip_y==ty);
   assert(head.face_x==dx*16&&head.face_y==dy*16);
   // The working edge moves into contact along its own normal; the poll or
   // broad back cannot be the leading surface of a sideways strike.
   auto approach=art::tool_anim::motion(d,tool,duration-duration*128/256,grade,tx,ty,true);
   const auto &before=art::tool_anim::held_frame(tool,grade,approach.angle,approach.mirrored);
   int vx=tx-(approach.x+before.tip_x),vy=ty-(approach.y+before.tip_y);
   assert(vx*dx+vy*dy>0&&std::abs(vx*dy-vy*dx)<=1);
  }
  for(int ms=1;ms<=duration;++ms) {
   auto e=art::tool_anim::motion(1,tool,ms,grade),w=art::tool_anim::motion(3,tool,ms,grade);
   assert(e.x==-w.x&&e.y==w.y&&((e.angle+w.angle)&15)==0&&w.mirrored&&!e.mirrored);
   if(tool==4)for(int d=0;d<4;++d) {
    auto pose=art::tool_anim::motion(d,tool,ms,grade);
    const auto &blade=art::tool_anim::held_frame(tool,grade,pose.angle,pose.mirrored);
    assert(blade.face_x*((d==1)-(d==3))+blade.face_y*((d==2)-(d==0))>0);
   }
  }
 }
 // Characters are independent silhouettes, not four recolorings of the farmer.
 for(int a=-1;a<4;++a)for(int b=a+1;b<4;++b) {
  const auto &sa=a<0?art::person_sprite(2,0):art::villager_sprite(a,2);
  const auto &sb=art::villager_sprite(b,2);int different=0;
  for(int y=0;y<32;++y)for(int x=0;x<24;++x)different+=art::hit(sa,x,y)!=art::hit(sb,x,y);
  assert(different>=8);
 }
 for(int id=0;id<4;++id)for(int d=0;d<4;++d) {
  const auto &rig=art::villager_rig(id,d);assert(rig.body->w==24&&rig.body->h==32&&rig.side==bool(d&1));
  for(int leg=0;leg<2;++leg)assert(rig.legs[leg]->rows[30]>rig.legs[leg]->rows[29]);
  std::vector<uint16_t>a(48*48),b(a.size());art::Raster aa{a.data(),48,48},bb{b.data(),48,48};
  art::gait::draw(aa,24,40,d,1,false,&rig);art::gait::draw(bb,24,40,d,5,false,&rig);assert(a!=b);
  for(int frame=0;frame<=8;++frame) {
   std::fill(a.begin(),a.end(),0);art::gait::draw(aa,24,40,d,frame,false,&rig);
   constexpr auto floor=art::rgb(178,149,111);std::fill(b.begin(),b.end(),floor);
   art::gait::shadow(bb,24,40,d,frame,&rig);int contacts=0;
   for(int y=1;y<48;++y)for(int x=0;x<48;++x)if(b[y*48+x]==art::blend(floor,art::ShadowTint,3)) {
    assert(a[(y-1)*48+x]);++contacts;
   }
   assert(contacts);
  }
 }
 for(int d=0;d<4;++d) {
  for(int tool:{2,6}) {
   auto m=art::tool_anim::motion(d,tool,art::tool_anim::impact_remaining(tool));
   const auto &tip=art::tool_anim::held_frame(tool,0,m.angle,m.mirrored);
   assert(tip.face_x==((d==1)-(d==3))*16&&tip.face_y==((d==2)-(d==0))*16);
  }
  std::vector<uint16_t>a(80*80),b(a.size());art::Raster ra{a.data(),80,80},rb{b.data(),80,80};
  art::tool_anim::use(ra,40,50,d,0,6,1,0,true);art::tool_anim::use(rb,40,50,d,0,6,0,0,true);
  assert(a==b); // Settling into fishing cannot flip the rod or detach the line.
 }
 if(argc>1) {
  std::vector<uint16_t>p(240*176,art::Sky);art::Raster r{p.data(),240,176};
  for(int row=0;row<4;++row)for(int id=-1;id<4;++id){int x=(id+1)*48+24,y=row*44+37;auto*rig=id<0?nullptr:&art::villager_rig(id,row);art::gait::shadow(r,x,y,row,1,rig);art::gait::draw(r,x,y,row,1,false,rig);}
  ppm(std::string(argv[1])+"/characters.ppm",p,240,176,4);
  p.assign(480*256,art::Sky);r={p.data(),480,256};
  for(int d=0;d<4;++d)for(int i=0;i<10;++i){int x=i*48+24,y=d*64+48;int rem=art::tool_anim::duration(4)*(9-i)/9;
   art::person(r,x,y,d,0,false,rem,4,false,5);r.rect(x-2+((d==1)-(d==3))*16,y+((d==2)-(d==0))*16-art::tool_anim::surface_offset(4),5,1,farm_palette::WoodDark);}
  ppm(std::string(argv[1])+"/axe-poses.ppm",p,480,256,2);
 }
 puts("PASS: registered blade contacts, exact east/west pixel mirrors, 16-step tool arcs, distinct NPC silhouettes and shared alternating rig animation");
}
