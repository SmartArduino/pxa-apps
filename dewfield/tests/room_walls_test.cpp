// SPDX-License-Identifier: MIT
#include "../scene.hpp"
#include <cassert>
#include <cstdio>
#include <vector>
using namespace farm_scene;
constexpr int W=512,H=384;
static Queue architecture(farm::State&s,Rect clip={0,0,W,H}) {
 Queue scene;scene.build(s,{},clip);Queue walls;walls.viewport=clip;
 for(unsigned i=0;i<scene.count;++i)if(scene.entries[i].kind==Kind::RoomWall)walls.entries[walls.count++]=scene.entries[i];
 return walls;
}
int main(){
 constexpr auto ground=art::rgb(113,132,95);
 farm::State s;farm::fresh(s);s.px=264;s.py=296;
 std::vector<uint16_t> opaque(W*H),faded(W*H),body(W*H),combined(W*H);
 for(int region:{4,7,9,10,11,12}){
  s.region=region;auto walls=architecture(s);art::Raster r{opaque.data(),W,H};
  std::fill(opaque.begin(),opaque.end(),0);walls.objects(r,s,{});
  // Independent volume projection. Extrude the floor plan vertically by 28
  // pixels. A missing corner, phantom door header, unequal wall elevation or
  // painter-order hole must disagree with this solid-volume silhouette.
  std::vector<bool> floor(W*H),expected(W*H);
  for(int ty=5;ty<=12;++ty)for(int tx=7;tx<=24;++tx)if(farm::partition_wall(region,tx,ty)){
   auto fill=[&](int x,int y,int w,int h){for(int yy=y;yy<y+h;++yy)for(int xx=x;xx<x+w;++xx)floor[yy*W+xx]=true;};
   fill(tx*16+4,ty*16+4,8,8);
   if(ty==5||farm::partition_wall(region,tx,ty-1))fill(tx*16+4,ty*16,8,4);
   if(tx==24||farm::partition_wall(region,tx+1,ty))fill(tx*16+12,ty*16+4,4,8);
   if(farm::partition_wall(region,tx,ty+1))fill(tx*16+4,ty*16+12,8,4);
   if(tx==7||farm::partition_wall(region,tx-1,ty))fill(tx*16,ty*16+4,4,8);
  }
  for(int y=28;y<H;++y)for(int x=0;x<W;++x)if(floor[y*W+x])for(int elevation=0;elevation<=28;++elevation)expected[(y-elevation)*W+x]=true;
  // Moulded capitals overhang the structural wall by two pixels, as in the
  // approved concept; they do not change collision or the wall-foot depth.
  for(int ty=5;ty<=12;++ty)for(int tx=7;tx<=24;++tx)if(farm::partition_wall(region,tx,ty)){
   auto n=farm::walls::links(region,tx,ty);
   if(n==(farm::walls::North|farm::walls::South)||n==(farm::walls::East|farm::walls::West))continue;
   int left=n==farm::walls::North?3:2,top=n==farm::walls::North?8:2;
   int width=n==farm::walls::North?10:12,height=n==farm::walls::North?8:12;
   for(int y=0;y<height;++y)for(int x=0;x<width;++x)if(!((x==0||x==width-1)&&(y==0||y==height-1)))
    expected[(ty*16-28+top+y)*W+tx*16+left+x]=true;
  }
  for(unsigned i=0;i<opaque.size();++i)assert(bool(opaque[i])==expected[i]);
  if(region==7)for(int x:{12*16+8,18*16+8})
   assert(opaque[(12*16-28+4)*W+x]==room_wall_sprites::sample(room_wall_sprites::Post,6,2));
  // The two-tile doorway stays open between the timber jamb and lower capital.
  if(region!=7)for(int x:{15*16+8,18*16+8})for(int y=160;y<166;++y)assert(!opaque[y*W+x]);
  r.p=opaque.data();
  // Culling and nonzero camera origins must reproduce the full scene exactly.
  for(auto point:{farm::Point{101,156},farm::Point{187,163},farm::Point{241,129},farm::Point{291,174}}){
   constexpr int cw=23,ch=29;std::vector<uint16_t> frame(cw*ch+32,0xabcd);
   std::fill(frame.begin()+16,frame.end()-16,0);art::Raster clipped{frame.data()+16,cw,ch,point.x,point.y};
   auto q=architecture(s,{point.x,point.y,cw,ch});q.objects(clipped,s,{});
   for(int y=0;y<ch;++y)for(int x=0;x<cw;++x)assert(frame[16+y*cw+x]==opaque[(point.y+y)*W+point.x+x]);
   for(int i=0;i<16;++i)assert(frame[i]==0xabcd&&frame[16+cw*ch+i]==0xabcd);
  }
 }
 s.region=4;s.px=192;s.py=184;auto walls=architecture(s);
 std::fill(opaque.begin(),opaque.end(),ground);art::Raster r{opaque.data(),W,H};auto far=s;far.px=264;far.py=296;walls.objects(r,far,{});
 std::fill(faded.begin(),faded.end(),ground);r.p=faded.data();walls.objects(r,s,{});
 int samples=0;
 for(int y=s.py-30;y<s.py-2;++y)for(int x=s.px-10;x<s.px+10;++x)if(opaque[y*W+x]!=ground){
  assert(faded[y*W+x]==art::blend(ground,opaque[y*W+x],3));++samples;
 }
 assert(samples>100); // Includes the seam at x=192, each pixel blended once.
 for(int py:{184,216}){
  s.px=200;s.py=py;Queue actor;actor.viewport={0,0,W,H};actor.add(Kind::Player,s.px,s.py,s.py);
  std::fill(body.begin(),body.end(),0);r.p=body.data();actor.objects(r,s,{});
  Queue q=walls;q.add(Kind::Player,s.px,s.py,s.py);q.sort();std::fill(combined.begin(),combined.end(),0);r.p=combined.data();q.objects(r,s,{});
  int overlap=0,visible=0;
  for(unsigned i=0;i<body.size();++i)if(body[i]){
   if(py>204)assert(combined[i]==body[i]);
   if(opaque[i]!=ground){++overlap;visible+=combined[i]!=opaque[i];}
  }
  if(py<204)assert(overlap>4&&visible*4>overlap*3);
 }
 puts("PASS: uniform wall volume, connected caps, true openings, single-blend cutaway, actor depth and camera clipping");
}
