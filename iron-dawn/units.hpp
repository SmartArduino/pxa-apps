// SPDX-License-Identifier: MIT
#pragma once
#include "model.hpp"
#include "raster.hpp"

namespace iron {
inline constexpr std::array<Pt,16> unit_directions={{{1,0},{.9238795f,.3826834f},{.7071068f,.7071068f},{.3826834f,.9238795f},{0,1},{-.3826834f,.9238795f},{-.7071068f,.7071068f},{-.9238795f,.3826834f},{-1,0},{-.9238795f,-.3826834f},{-.7071068f,-.7071068f},{-.3826834f,-.9238795f},{0,-1},{.3826834f,-.9238795f},{.7071068f,-.7071068f},{.9238795f,-.3826834f}}};
inline int unit_facing(float radians){return int(std::lround(radians*8/3.14159265f))&15;}

// Body geometry is shared by the battlefield and production portraits.
// Light remains fixed in screen space as a vehicle turns.
struct UnitPainter {
 Raster& r;int x,y;float zoom,dx,dy;uint16_t team;bool flash;int pose;
 Pt at(float forward,float side,float z=0)const{float xx=dx*forward-dy*side,yy=dy*forward+dx*side;return {float(x+std::lround((xx-yy)*20*zoom)),float(y+std::lround(((xx+yy)*10-z)*zoom))};}
 uint16_t armor(uint16_t c)const{return flash?mix(c,cream,7):c;}
 void line(Pt a,Pt b,uint16_t c,int thickness=1){r.line(x+std::lround(a.x-x),y+std::lround(a.y-y),x+std::lround(b.x-x),y+std::lround(b.y-y),c,std::max(1,int(std::lround(thickness*zoom))));}
 void surface(float a,float b,float c,float d,float z,uint16_t color){r.poly({at(a,b,z),at(c,b,z),at(c,d,z),at(a,d,z)},color);}
 void block(float a,float b,float c,float d,float base,float top,uint16_t color){
  auto p=std::array<Pt,4>{at(a,b,top),at(c,b,top),at(c,d,top),at(a,d,top)};
  // Two visible faces are selected from the fixed isometric light direction.
  auto face=[&](int i,int j,float aa,float bb,float cc,float dd){if(p[i].x>p[j].x)r.poly({p[i],p[j],at(cc,dd,base),at(aa,bb,base)},mix(color,navy,8));};
  face(0,1,a,b,c,b);face(1,2,c,b,c,d);face(2,3,c,d,a,d);face(3,0,a,d,a,b);
  r.poly(p,armor(color));
  for(int i=0;i<4;++i){auto a1=p[i],b1=p[(i+1)%4];line(a1,b1,b1.x>a1.x?mix(color,cream,5):mix(color,navy,3));}
 }
 void mark(float forward,float side,float z,uint16_t color,int radius=1){auto p=at(forward,side,z);int size=std::max(1,int(radius*zoom));r.rect(std::lround(p.x),std::lround(p.y),size,size,color);}
 void cannon(float z,float reach,float rise,bool brake){
  auto pivot=at(-.06f,0,z),tip=at(reach-(flash?.08f:0),0,z+rise);
  line(pivot,tip,rgb(22,31,31),5);line(pivot,tip,armor(rgb(167,179,153)),2);
  float vx=tip.x-pivot.x,vy=tip.y-pivot.y,len=std::max(1.f,std::hypot(vx,vy));Pt normal={-vy/len*2.2f*zoom,vx/len*2.2f*zoom};
  if(brake){Pt collar={pivot.x+vx*.87f,pivot.y+vy*.87f};line({collar.x-normal.x,collar.y-normal.y},{collar.x+normal.x,collar.y+normal.y},gold,2);}
  line({tip.x-normal.x,tip.y-normal.y},{tip.x+normal.x,tip.y+normal.y},rgb(26,36,35));
 }
 void infantry(int kind){
  int step=pose==1?2:pose==2?-2:0;uint16_t suit=kind==rocket?rgb(99,115,112):rgb(111,132,91),dark=rgb(37,49,46);
  auto xx=[&](int n){return x+int(std::lround(n*zoom));};auto yy=[&](int n){return y+int(std::lround(n*zoom));};
  auto patch=[&](int px,int py,int w,int h,uint16_t c){r.rect(xx(px),yy(py),std::max(1,int(std::lround(w*zoom))),std::max(1,int(std::lround(h*zoom))),c);};
  r.ellipse(xx(2),yy(2),std::max(1,int(5*zoom)),std::max(1,int(2*zoom)),rgb(26,38,33));
  auto foot=[&](int side,int stride){r.line(xx(side*2),yy(-5),xx(side*2+stride),yy(1),dark,std::max(1,int(2*zoom)));patch(side*2+stride-1,0,3,2,rgb(20,29,29));};foot(-1,step);foot(1,-step);
  // Backpack, flak vest, shoulder patches and separate hands.
  patch(-5,-16,4,11,rgb(118,107,72));patch(-4,-15,8,9,dark);patch(-5,-13,2,6,rgb(166,143,91));patch(-3,-15,6,10,armor(suit));patch(-2,-13,4,5,mix(suit,navy,5));
  r.line(xx(-3),yy(-6),xx(3),yy(-6),rgb(151,159,121));patch(-5,-14,2,5,team);patch(3,-14,2,5,kind==rocket?gold:armor(suit));
  r.ellipse(x,yy(-18),std::max(1,int(4*zoom)),std::max(1,int(3*zoom)),rgb(34,48,42));patch(-3,-21,6,3,armor(rgb(160,173,136)));
  patch(-2,-18,4,2,rgb(191,159,113));patch(-2,-18,4,1,rgb(37,106,113));patch(dx-dy>0?0:-2,-18,2,1,cyan);patch(-3,-3,2,2,rgb(134,145,111));patch(1,-3,2,2,rgb(134,145,111));
  if(kind==rocket){
   auto back=at(-.25f,.06f,13),front=at(.57f,.06f,13);line(back,front,dark,5);line(back,front,armor(rgb(154,160,124)),3);line(at(.32f,.06f,13),at(.38f,.06f,13),gold,4);
   auto end=at(.57f,.06f,13);r.ellipse(end.x,end.y,std::max(1,int(2*zoom)),std::max(1,int(2*zoom)),navy);patch(-4,-11,2,2,rgb(193,158,111));
  }else{auto gun=at(.5f,0,11);line(at(-.04f,0,11),gun,dark,2);mark(.5f,0,11,rgb(152,164,145));patch(2,-10,2,2,rgb(193,158,111));}
 }
 void tracks(float length,float width){
  for(int s:{-1,1}){
   float edge=s*width;block(-length,edge-.115f,length,edge+.115f,0,4,rgb(38,48,44));
   for(int i=0;i<6;++i){float f=-length+.1f+i*(length*2-.2f)/5;auto p=at(f,edge,3);r.rect(p.x,p.y,std::max(1,int(3*zoom)),std::max(1,int(2*zoom)),rgb(94,106,89));}
   for(int i=0;i<5;++i){float f=-length+.08f+i*(length*2-.05f)/5+(pose==1?.045f:0);line(at(f,edge-.09f,5),at(f,edge+.09f,5),rgb(127,136,111));}
  }
 }
 void vehicle(int kind,bool loaded){
  r.ellipse(x+int(std::lround(3*zoom)),y+int(std::lround(3*zoom)),18*zoom,7*zoom,rgb(26,38,33));
  if(kind==harvester){
   // Six wheels, amber ore hopper and a glazed cab give the miner its own silhouette.
   for(int s:{-1,1})for(int i=0;i<3;++i){auto q=at(-.52f+i*.5f,s*.47f,3);r.ellipse(q.x,q.y,4*zoom,3*zoom,rgb(25,35,32));r.ellipse(q.x,q.y,2*zoom,2*zoom,rgb(128,137,112));}
   block(-.68f,-.43f,.7f,.43f,3,8,rgb(147,126,74));block(-.64f,-.39f,.12f,.39f,8,17,rgb(187,151,79));surface(-.55f,-.29f,.03f,.29f,18,rgb(58,71,64));
   if(loaded)for(int i=0;i<4;++i){auto q=at(-.46f+(i%2)*.28f,-.16f+(i/2)*.28f,18+i%2*2);r.poly({Pt{q.x-4*zoom,q.y},Pt{q.x,q.y-5*zoom},Pt{q.x+4*zoom,q.y},Pt{q.x,q.y+2*zoom}},i%2?gold:rgb(219,184,111));mark(-.46f+(i%2)*.28f,-.16f+(i/2)*.28f,22,cream);}
   block(.16f,-.34f,.6f,.34f,8,21,rgb(190,155,85));surface(.25f,-.28f,.56f,.28f,22,rgb(61,126,133));line(at(.45f,-.28f,23),at(.45f,.28f,23),cyan);
   r.poly({at(.61f,-.27f,13),at(.61f,.27f,13),at(.61f,.27f,20),at(.61f,-.27f,20)},rgb(43,103,115));line(at(.62f,-.2f,19),at(.62f,.05f,15),cyan);
   for(int i=0;i<3;++i){mark(.27f,-.23f+i*.23f,24,navy,3);mark(.28f,-.23f+i*.23f,25,cream,2);}
   block(.72f,-.5f,.91f,.5f,0,5,rgb(148,159,130));line(at(.91f,-.5f,5),at(.91f,.5f,5),gold,2);for(int i=0;i<4;++i)line(at(.91f,-.39f+i*.26f,3),at(1.f,-.39f+i*.26f,1),rgb(185,196,163),2);
   for(int i=0;i<3;++i)line(at(-.55f+i*.19f,.41f,10),at(-.46f+i*.19f,.41f,15),navy,2);
   surface(.13f,-.35f,.2f,.35f,22,team);mark(.62f,-.36f,12,cream,2);mark(.62f,.36f,12,cream,2);mark(-.65f,.3f,9,red,2);return;
  }
  bool longgun=kind==artillery;float length=longgun?.72f:.61f;tracks(length,.48f);
  block(-length,-.35f,length,.35f,4,9,longgun?rgb(99,117,101):rgb(128,146,105));
  // Sloped glacis, side armor plates, rear engine grille and headlamps.
  r.poly({at(.22f,-.33f,11),at(length,-.3f,7),at(length,.3f,7),at(.22f,.33f,11)},armor(rgb(157,169,130)));
  surface(-length+.06f,-.32f,-length+.2f,.32f,10,team);
  for(int i=0;i<4;++i){float f=-length+.05f+i*.07f;line(at(f,-.21f,11),at(f,.21f,11),rgb(37,54,49));}
  for(int s:{-1,1}){for(int i=0;i<3;++i)block(-.5f+i*.33f,s*.34f-.035f,-.25f+i*.33f,s*.34f+.035f,5,10,rgb(96,114,88));mark(.54f,s*.29f,10,cream,2);mark(-length,s*.27f,8,gold,2);}
  if(longgun){
   block(-.32f,-.25f,.25f,.25f,9,15,rgb(80,103,91));block(-.32f,-.21f,-.05f,.21f,15,20,rgb(142,155,126));
   surface(-.3f,-.18f,-.08f,.18f,21,gold);line(at(-.2f,-.23f,17),at(.23f,-.23f,17),team,2);
   for(int side:{-1,1}){line(at(-.51f,side*.29f,9),at(-.1f,side*.22f,21),rgb(28,44,39),4);line(at(-.51f,side*.29f,9),at(-.1f,side*.22f,21),rgb(147,161,129),2);mark(-.1f,side*.22f,21,gold,2);line(at(-.64f,side*.32f,8),at(-.83f,side*.45f,1),rgb(160,171,140),2);}
   block(-.43f,-.18f,-.26f,.18f,16,23,rgb(111,130,111));cannon(18,1.55f,10,true);block(-.85f,-.34f,-.69f,.34f,0,4,rgb(72,88,74));
  }else{
   block(-.24f,-.27f,.29f,.27f,9,17,rgb(150,165,125));surface(-.22f,-.28f,-.11f,.28f,18,team);
   auto hatch=at(-.09f,0,19);r.ellipse(hatch.x,hatch.y,5*zoom,3*zoom,rgb(39,58,47));r.ellipse(hatch.x,hatch.y-int(std::lround(zoom)),4*zoom,2*zoom,rgb(170,181,143));line(at(-.14f,-.08f,21),at(-.02f,-.08f,21),cream);
   for(int side:{-1,1}){line(at(-.04f,side*.28f,12),at(.06f,side*.28f,16),team,2);line(at(.12f,side*.28f,12),at(.22f,side*.28f,16),team,2);}
   cannon(16,.99f,0,false);auto antenna=at(-.22f,.21f,18);line(antenna,{antenna.x,antenna.y-8*zoom},rgb(165,181,146));
  }
 }
 void draw(int kind,bool loaded){if(kind<=rocket)infantry(kind);else vehicle(kind,loaded);}
};

// A bounded LRU stores opaque color runs, so detail is paid for on a cache miss.
// Dynamic selection, health, dust and veteran marks remain outside the cache.
struct UnitSpriteCache {
 static constexpr int slots=48,max_runs=1024,tile_w=128,tile_h=104,origin_x=64,origin_y=76;
 struct Run {uint16_t color;uint8_t x,y,length;};
 struct Entry {uint64_t key=0;uint32_t used=0;unsigned count=0;std::array<Run,max_runs> runs{};};
 std::array<Entry,slots> entries{};std::array<uint16_t,tile_w*tile_h> scratch{};uint32_t epoch=0;unsigned hits=0,misses=0;
 static uint64_t key(int kind,int team,int facing,int zoom,int pose,bool loaded,bool flash){return 1+uint64_t(kind|(team<<3)|(facing<<4)|(pose<<8)|(loaded<<10)|(flash<<11)|(zoom<<12));}
 void draw(Raster& r,int x,int y,int kind,int team,int facing,float zoom,int pose,bool loaded,bool flash){
  if(zoom<.4f||zoom>1.35f){UnitPainter{r,x,y,zoom,unit_directions[facing].x,unit_directions[facing].y,team?red:cyan,flash,pose}.draw(kind,loaded);return;}
  int z=std::lround(zoom*1000);auto id=key(kind,team,facing,z,pose,loaded,flash);Entry* item=nullptr;auto oldest=&entries[0];
  for(auto& e:entries){if(e.key==id){item=&e;break;}if(e.used<oldest->used)oldest=&e;}
  if(item)++hits;else{
   ++misses;item=oldest;item->key=0;item->count=0;scratch.fill(0);Raster tile(scratch.data(),tile_w,tile_h,tile_w);
   UnitPainter{tile,origin_x,origin_y,z/1000.f,unit_directions[facing].x,unit_directions[facing].y,team?red:cyan,flash,pose}.draw(kind,loaded);
   for(int yy=0;yy<tile_h;++yy)for(int xx=0;xx<tile_w;){uint16_t color=scratch[yy*tile_w+xx];if(!color){++xx;continue;}int left=xx++;while(xx<tile_w&&scratch[yy*tile_w+xx]==color)++xx;if(item->count==max_runs){UnitPainter{r,x,y,zoom,unit_directions[facing].x,unit_directions[facing].y,team?red:cyan,flash,pose}.draw(kind,loaded);return;}item->runs[item->count++]={color,uint8_t(left),uint8_t(yy),uint8_t(xx-left)};}
   item->key=id;
  }
  item->used=++epoch;for(unsigned i=0;i<item->count;++i){auto& run=item->runs[i];r.rect(x+run.x-origin_x,y+run.y-origin_y,run.length,1,run.color);}
 }
};
inline UnitSpriteCache unit_sprites;
}
