// SPDX-License-Identifier: MIT
#pragma once
#include "geometry.hpp"
#include "../common/motion-games/paint.hpp"
namespace racer {
using motion::Paint;using motion::Rect;using motion::rgb;using motion::Point;
struct Palette {uint16_t sky,sky_low,grass,grass_alt,road,road_alt,mountain,accent;};
inline constexpr Palette palettes[]={
 {rgb(125,204,224),rgb(203,237,226),rgb(119,183,145),rgb(130,194,148),rgb(79,104,119),rgb(85,111,123),rgb(104,163,162),rgb(248,218,148)},
 {rgb(149,201,217),rgb(220,238,217),rgb(114,173,138),rgb(127,185,143),rgb(82,98,113),rgb(91,108,120),rgb(95,153,152),rgb(243,210,136)},
 {rgb(34,42,79),rgb(99,99,144),rgb(70,82,112),rgb(77,89,120),rgb(51,57,87),rgb(57,66,94),rgb(63,64,103),rgb(230,159,211)}};
inline constexpr uint16_t car_colors[]={rgb(239,119,103),rgb(100,185,214),rgb(170,142,218),rgb(246,204,110)};
struct RoadPainter {
 Camera camera;ClipRect clip;unsigned width=240;int top=40,bottom=250;
 void polygon(Paint& p,std::span<const V3> points,uint16_t color){std::array<V3,12> near{};unsigned count=clip_near(points,near,camera.near);if(count<3)return;
   std::array<P2,12> projected{},clipped{};for(unsigned i=0;i<count;++i)projected[i]=camera.project(near[i]);count=clip_screen(std::span(projected).first(count),clip,clipped);if(count<3)return;
   for(unsigned i=1;i+1<count;i+=2){unsigned last=std::min(i+2,count-1);p.quad({Point{clipped[0].x,clipped[0].y},Point{clipped[i].x,clipped[i].y},Point{clipped[i+1].x,clipped[i+1].y},Point{clipped[last].x,clipped[last].y}},color);}}
 void quad(Paint& p,std::array<V3,4> vertices,uint16_t color){polygon(p,vertices,color);}
 void screen_polygon(Paint& p,std::array<P2,4> points,uint16_t color){std::array<P2,12> out{};unsigned n=clip_screen(points,clip,out);if(n<3)return;
   for(unsigned i=1;i+1<n;i+=2){auto last=std::min(i+2,n-1);p.quad({Point{out[0].x,out[0].y},Point{out[i].x,out[i].y},Point{out[i+1].x,out[i+1].y},Point{out[last].x,out[last].y}},color);}}
 void strip(Paint& p,const WorldFrame& near,const WorldFrame& far,float left,float right,uint16_t color){quad(p,{camera.point(far,left),camera.point(far,right),camera.point(near,right),camera.point(near,left)},color);}
 static uint16_t shade(uint16_t color,int amount){return rgb(unsigned(std::clamp(int(color>>11)*8+amount,0,255)),unsigned(std::clamp(int((color>>5)&63)*4+amount,0,255)),unsigned(std::clamp(int(color&31)*8+amount,0,255)));}
 void car(Paint& p,const Track& track,float station,float lane,uint16_t color,float yaw=0,bool player=false){auto frame=frame_at(track,station);auto center=camera.point(frame,lane*6);if(center.z<camera.near+2||center.z>450)return;
   float ca=std::cos(yaw),sa=std::sin(yaw),scale=1.16f;auto point=[&](float x,float y,float z){x*=scale;y*=scale;z*=scale;return camera.point(frame,lane*6+x*ca+z*sa,y,-x*sa+z*ca);};
   auto face=[&](std::array<V3,4> vertices,uint16_t c){auto a=vertices[0],b=vertices[1],d=vertices[2];V3 u{b.x-a.x,b.y-a.y,b.z-a.z},v{d.x-a.x,d.y-a.y,d.z-a.z};float facing=(u.y*v.z-u.z*v.y)*a.x+(u.z*v.x-u.x*v.z)*a.y+(u.x*v.y-u.y*v.x)*a.z;if(facing<0)quad(p,vertices,c);};
   // Cache the eight transformed corners and cull rear-facing box faces. This
   // pays for glass, rims and body details without drawing invisible surfaces.
   auto box=[&](float x1,float x2,float y1,float y2,float z1,float z2,uint16_t side,uint16_t roof){
     std::array<V3,8> v{point(x1,y1,z1),point(x2,y1,z1),point(x2,y2,z1),point(x1,y2,z1),point(x1,y1,z2),point(x2,y1,z2),point(x2,y2,z2),point(x1,y2,z2)};
     face({v[4],v[5],v[6],v[7]},side);face({v[0],v[4],v[7],v[3]},side);face({v[5],v[1],v[2],v[6]},side);face({v[1],v[0],v[3],v[2]},side);face({v[3],v[7],v[6],v[2]},roof);};
   quad(p,{point(-.95f,.02f,-1.7f),point(-.95f,.02f,1.6f),point(.95f,.02f,1.6f),point(.95f,.02f,-1.7f)},rgb(52,74,86));
   for(float z:{-.94f,.85f})for(float x:{-.74f,.74f}){box(x-.17f,x+.17f,.09f,.51f,z-.33f,z+.33f,rgb(32,43,54),rgb(47,59,69));
     float xx=x+(x<0?-.175f:.175f);if(x<0)face({point(xx,.21f,z-.14f),point(xx,.21f,z+.14f),point(xx,.39f,z+.14f),point(xx,.39f,z-.14f)},rgb(182,200,202));else face({point(xx,.21f,z+.14f),point(xx,.21f,z-.14f),point(xx,.39f,z-.14f),point(xx,.39f,z+.14f)},rgb(182,200,202));}
   box(-.70f,.70f,.30f,.79f,-1.48f,1.48f,shade(color,-20),color);
   // Sloped glazing and closed cabin sides make a complete solid silhouette.
   face({point(-.61f,.80f,.9f),point(.61f,.80f,.9f),point(.47f,1.28f,.43f),point(-.47f,1.28f,.43f)},rgb(156,209,219));
   face({point(-.61f,.80f,-1.02f),point(-.61f,.80f,.9f),point(-.47f,1.28f,.43f),point(-.47f,1.28f,-.59f)},rgb(86,135,158));
   face({point(.61f,.80f,.9f),point(.61f,.80f,-1.02f),point(.47f,1.28f,-.59f),point(.47f,1.28f,.43f)},rgb(102,157,175));
   quad(p,{point(-.47f,1.28f,-.59f),point(-.47f,1.28f,.43f),point(.47f,1.28f,.43f),point(.47f,1.28f,-.59f)},shade(color,20));
   face({point(-.61f,.80f,-1.02f),point(-.47f,1.28f,-.59f),point(.47f,1.28f,-.59f),point(.61f,.80f,-1.02f)},rgb(52,85,109));
   quad(p,{point(.65f,.31f,-1.49f),point(-.65f,.31f,-1.49f),point(-.65f,.42f,-1.49f),point(.65f,.42f,-1.49f)},rgb(47,59,72));
   for(float x:{-.58f,.35f})quad(p,{point(x,.49f,-1.50f),point(x+.23f,.49f,-1.50f),point(x+.23f,.65f,-1.50f),point(x,.65f,-1.50f)},rgb(249,113,103));
   quad(p,{point(-.16f,.45f,-1.51f),point(.16f,.45f,-1.51f),point(.16f,.58f,-1.51f),point(-.16f,.58f,-1.51f)},rgb(234,238,224));
   if(player)for(float x:{-.25f,.08f}){quad(p,{point(x,.805f,.89f),point(x+.14f,.805f,.89f),point(x+.14f,.805f,1.47f),point(x,.805f,1.47f)},motion::white);quad(p,{point(x,1.29f,-.59f),point(x,1.29f,.43f),point(x+.14f,1.29f,.43f),point(x+.14f,1.29f,-.59f)},motion::white);}
 }
 void scenery(Paint& p,const Track& track,float station,unsigned theme){auto frame=frame_at(track,station);float side=((int(station/30)&1)?-1.f:1.f),offset=side*9;
   auto billboard=[&](float x,float y,float width,float height,uint16_t c){quad(p,{camera.point(frame,x-width*.5f,y),camera.point(frame,x+width*.5f,y),camera.point(frame,x+width*.5f,y+height),camera.point(frame,x-width*.5f,y+height)},c);};
   if(theme==2){float height=5+float(int(station/45)%4);uint16_t color=rgb(79+unsigned(int(station/45)%3)*12,77,123);billboard(offset,0,3,height,color);
     for(int i=1;i<4;++i)billboard(offset,i*1.35f,1.8f,.38f,i&1?rgb(247,202,137):rgb(132,211,214));
     float lamp=-side*7.4f;billboard(lamp,0,.12f,4.2f,rgb(120,128,150));billboard(lamp-side*.5f,4.05f,1.1f,.13f,rgb(165,171,186));billboard(lamp-side*.9f,3.85f,.3f,.25f,rgb(252,222,163));
   }else if(theme==0){
     if(int(station/48)&1){billboard(offset,0,.16f,2.5f,rgb(177,139,91));quad(p,{camera.point(frame,offset-2,2.2f),camera.point(frame,offset,3.2f),camera.point(frame,offset+2,2.2f),camera.point(frame,offset+2,2.2f)},rgb(244,212,149));quad(p,{camera.point(frame,offset,3.2f),camera.point(frame,offset+.7f,2.2f),camera.point(frame,offset-.7f,2.2f),camera.point(frame,offset-.7f,2.2f)},rgb(93,169,167));}
     else {quad(p,{camera.point(frame,offset-.18f,0),camera.point(frame,offset+.18f,0),camera.point(frame,offset+.5f,4.3f),camera.point(frame,offset+.25f,4.3f)},rgb(165,128,82));
       for(int i=-1;i<=1;++i){float x=offset+.35f;quad(p,{camera.point(frame,x,4.5f),camera.point(frame,x+i*2.7f-1.5f,3.5f+std::abs(i)*.15f),camera.point(frame,x+i*2.2f-.3f,4.15f),camera.point(frame,x,4.5f)},i==0?rgb(72,145,119):rgb(87,161,126));}}
   }else{billboard(offset,0,.30f,3.2f,rgb(134,113,80));for(int i=0;i<2;++i){float base=1.5f+i*1.3f;quad(p,{camera.point(frame,offset-1.8f+i*.3f,base),camera.point(frame,offset+1.8f-i*.3f,base),camera.point(frame,offset,base+2.5f),camera.point(frame,offset,base+2.5f)},i?rgb(122,183,148):rgb(70,137,119));}
     quad(p,{camera.point(frame,-side*8-1.1f,0),camera.point(frame,-side*8+1.1f,0),camera.point(frame,-side*8+.3f,.9f),camera.point(frame,-side*8-.6f,.65f)},rgb(161,170,159));}
 }
 void draw(Paint& p,const Race& race,Rect area,unsigned color,bool preview=false,float animation=0){
   width=unsigned(p.width);top=area.y;bottom=area.y+area.h;clip={0,float(top),float(width),float(bottom)};camera.setup(race,int(width),top,bottom,preview);auto palette=palettes[race.track.id];
   // The frame starts in the grass color. Only paint the sky above it;
   // road and curb strips cover disjoint regions to avoid repeated fill.
   int horizon=std::clamp(int(camera.horizon),top+10,bottom-20);p.rect(0,top,int(width),horizon-top,palette.sky);
   p.rect(0,top+(horizon-top)/2,int(width),(horizon-top+1)/2,palette.sky_low);
   if(race.track.id==2){for(int i=0;i<12;++i){int x=(i*71+13)%int(width),y=top+8+(i*13%std::max(10,horizon-top-12));p.rect(x,y,1+(i&1),1+(i&1),rgb(242,220,183));}p.circle(int(width)*4/5,top+18,7,rgb(247,219,165));}
   else{p.circle(int(width)*4/5,top+19,9,palette.accent);for(int i=0;i<2;++i){int x=(int(width)*i/2+int(animation*3)+16)%int(width),y=top+14+(i&1)*10;auto cloud=rgb(238,248,237);p.round({x-11,y,38,8},cloud,3);p.quad({Point{float(x-6),float(y+2)},Point{float(x-3),float(y-4)},Point{float(x+5),float(y-5)},Point{float(x+11),float(y+2)}},cloud);p.quad({Point{float(x+8),float(y+2)},Point{float(x+11),float(y-2)},Point{float(x+18),float(y-1)},Point{float(x+23),float(y+2)}},cloud);}}
   if(race.track.id==0){p.rect(0,horizon-2,int(width),6,rgb(91,176,194));p.rect(0,horizon+4,int(width),2,rgb(174,218,207));}

   if(race.track.id==2){for(int i=0;i<8;++i){int x=i*int(width)/7-8,height=12+(i*13%19);p.rect(x,horizon-height,18+(i%3)*4,height,palette.mountain);p.rect(x+4,horizon-height+4,2,2,rgb(221,188,151));p.rect(x+10,horizon-5,2,2,rgb(142,180,190));}}
   else for(int i=0;i<4;++i){float x=float(int(width)*i/3)-25,peak=float(horizon-(race.track.id==0?9:25)-(i&1)*(race.track.id==0?7:15));screen_polygon(p,{P2{x-45,float(horizon)},P2{x+20,peak},P2{x+95,float(horizon)},P2{x+95,float(horizon)}},palette.mountain);if(race.track.id==1)screen_polygon(p,{P2{x+4,peak+8},P2{x+20,peak},P2{x+36,peak+8},P2{x+36,peak+8}},rgb(216,231,215));}
   constexpr unsigned bands=32;std::array<float,bands+1> depths{};std::array<WorldFrame,bands+1> frames{};
   float nearest=camera.focal*5/std::max(20.f,float(bottom)-camera.horizon)*.52f;for(unsigned i=0;i<=bands;++i){depths[i]=nearest;frames[i]=frame_at(race.track,camera.station+nearest);nearest*=1.145f;}
   // Sort the three opponents once per frame, with a fixed comparison network.
   std::array<unsigned,3> order{0,1,2};auto compare=[&](unsigned a,unsigned b){if(race.traffic[order[a]].distance<race.traffic[order[b]].distance)std::swap(order[a],order[b]);};compare(0,1);compare(1,2);compare(0,1);
   for(int i=int(bands)-1;i>=0;--i){auto &a=frames[unsigned(i)];auto &b=frames[unsigned(i)+1];auto near=camera.point(a,0),far=camera.point(b,0);if(near.z<camera.near||far.z<camera.near)continue;
     float ya=camera.project(near).y,yb=camera.project(far).y;if(ya-yb<.65f||ya<top||yb>bottom)continue;
     bool light=(int(a.center.z/20)&1)==0;
     auto curb=light?rgb(250,229,200):rgb(217,117,104);strip(p,a,b,-6.6f,-6,curb);strip(p,a,b,6,6.6f,curb);strip(p,a,b,-6,6,light?palette.road:palette.road_alt);
     if(light){strip(p,a,b,-2.08f,-1.92f,rgb(224,233,226));strip(p,a,b,1.92f,2.08f,rgb(224,233,226));}
     for(unsigned lap=1;lap<=2;++lap){float station=race.track.length*lap;if(station>a.center.z&&station<=b.center.z){auto line=frame_at(race.track,station),end=frame_at(race.track,station+2);for(int tile=0;tile<6;++tile)strip(p,line,end,-6+tile*2.f,-4+tile*2.f,tile&1?rgb(243,239,214):rgb(43,57,67));}}
     if(int(a.center.z/48)!=int(b.center.z/48))scenery(p,race.track,std::floor(b.center.z/48)*48,race.track.id);
     for(unsigned pad=0;pad<6;++pad){float station=race.pad_station(pad);if(station>a.center.z&&station<=b.center.z&&!(race.pads&(1u<<pad))){auto frame=frame_at(race.track,station),front=frame_at(race.track,station+6);float lane=race.pad_lane(pad)*6;strip(p,frame,front,lane-1.0f,lane+1.0f,rgb(105,218,219));
       quad(p,{camera.point(frame,lane-.7f,.03f),camera.point(frame,lane+.7f,.03f),camera.point(front,lane,.03f),camera.point(front,lane,.03f)},rgb(234,255,236));}}
     for(unsigned j:order){auto traffic=race.traffic[j];if(traffic.distance>a.center.z&&traffic.distance<=b.center.z)car(p,race.track,traffic.distance,traffic.lane,car_colors[(j+1)%4]);}
     if(race.distance>a.center.z&&race.distance<=b.center.z){bool blink=race.collision_cooldown>.1f&&(int(race.elapsed*14)&1);if(!blink)car(p,race.track,race.distance,race.x,car_colors[color%3],-race.steering*.10f,true);}
   }
   if(race.boost>0&&!preview){for(int i=0;i<5;++i){int x=int(width)/2+(i-2)*int(width)/6,yy=bottom-12-(int(animation*110+i*29)%std::max(20,area.h/3));p.line({float(x),float(yy)},{float(x+(i-2)*2),float(yy+13)},1,rgb(219,250,231));}}
 }
};
}
