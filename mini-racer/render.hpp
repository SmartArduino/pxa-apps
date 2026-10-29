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
 void car(Paint& p,const Track& track,float station,float lane,uint16_t color,float yaw=0,bool player=false){auto frame=frame_at(track,station);auto center=camera.point(frame,lane*6);if(center.z<camera.near+2||center.z>450)return;
   float ca=std::cos(yaw),sa=std::sin(yaw);auto point=[&](float x,float y,float z){return camera.point(frame,lane*6+x*ca+z*sa,y,-x*sa+z*ca);};
   auto box=[&](float x1,float x2,float y1,float y2,float z1,float z2,uint16_t side,uint16_t roof){
     // World-space low-poly cuboids; far-facing front/side faces precede the
     // rear face and roof in this chase camera's painter order.
     quad(p,{point(x1,y1,z2),point(x2,y1,z2),point(x2,y2,z2),point(x1,y2,z2)},side);
     quad(p,{point(x1,y1,z1),point(x1,y1,z2),point(x1,y2,z2),point(x1,y2,z1)},side);
     quad(p,{point(x2,y1,z2),point(x2,y1,z1),point(x2,y2,z1),point(x2,y2,z2)},side);
     quad(p,{point(x2,y1,z1),point(x1,y1,z1),point(x1,y2,z1),point(x2,y2,z1)},side);
     quad(p,{point(x1,y2,z1),point(x1,y2,z2),point(x2,y2,z2),point(x2,y2,z1)},roof);};
   // Four chunky rubber wheels stay visible around the sculpted body.
   for(float z:{-.95f,.85f})for(float x:{-.70f,.70f})box(x-.16f,x+.16f,.08f,.48f,z-.32f,z+.32f,rgb(34,45,61),rgb(55,66,79));
   box(-.68f,.68f,.28f,.81f,-1.48f,1.48f,color,color);
   quad(p,{point(-.61f,.82f,.9f),point(.61f,.82f,.9f),point(.47f,1.25f,.45f),point(-.47f,1.25f,.45f)},rgb(161,218,223));
   quad(p,{point(-.47f,1.25f,-.60f),point(-.47f,1.25f,.45f),point(.47f,1.25f,.45f),point(.47f,1.25f,-.60f)},color);
   quad(p,{point(-.61f,.82f,-1.02f),point(-.47f,1.25f,-.60f),point(.47f,1.25f,-.60f),point(.61f,.82f,-1.02f)},rgb(60,94,115));
   for(float x:{-.47f,.34f})quad(p,{point(x,.44f,-1.49f),point(x+.19f,.44f,-1.49f),point(x+.19f,.64f,-1.49f),point(x,.64f,-1.49f)},rgb(255,221,153));
   if(player)quad(p,{point(-.1f,.83f,.85f),point(.1f,.83f,.85f),point(.1f,.83f,1.4f),point(-.1f,.83f,1.4f)},motion::white);
 }
 void scenery(Paint& p,const Track& track,float station,unsigned theme){auto frame=frame_at(track,station);float side=((int(station/30)&1)?-1.f:1.f),offset=side*9;
   if(theme==2){float height=5+float(int(station/45)%4);uint16_t color=rgb(79+unsigned(int(station/45)%3)*12,77,123);
     quad(p,{camera.point(frame,offset-1.5f,0),camera.point(frame,offset+1.5f,0),camera.point(frame,offset+1.5f,height),camera.point(frame,offset-1.5f,height)},color);
     for(int i=1;i<3;++i)quad(p,{camera.point(frame,offset-.8f,i*1.5f),camera.point(frame,offset+.8f,i*1.5f),camera.point(frame,offset+.8f,i*1.5f+.5f),camera.point(frame,offset-.8f,i*1.5f+.5f)},i&1?rgb(243,198,123):rgb(117,203,212));
   }else{quad(p,{camera.point(frame,offset-.16f,0),camera.point(frame,offset+.16f,0),camera.point(frame,offset+.16f,3.2f),camera.point(frame,offset-.16f,3.2f)},rgb(134,113,80));
     for(int i=0;i<2;++i){float base=1.5f+i*1.3f;quad(p,{camera.point(frame,offset-1.8f+i*.3f,base),camera.point(frame,offset+1.8f-i*.3f,base),camera.point(frame,offset,base+2.5f),camera.point(frame,offset,base+2.5f)},i?rgb(112,177,144):rgb(73,143,125));}}
 }
 void draw(Paint& p,const Race& race,Rect area,unsigned color,bool preview=false,float animation=0){
   width=unsigned(p.width);top=area.y;bottom=area.y+area.h;clip={0,float(top),float(width),float(bottom)};camera.setup(race,int(width),top,bottom);auto palette=palettes[race.track.id];
   // The frame starts in the grass color. Only paint the sky above it;
   // road and curb strips cover disjoint regions to avoid repeated fill.
   int horizon=std::clamp(int(camera.horizon),top+10,bottom-20);p.rect(0,top,int(width),horizon-top,palette.sky);
   p.rect(0,top+(horizon-top)/2,int(width),(horizon-top+1)/2,palette.sky_low);
   if(race.track.id==2){for(int i=0;i<12;++i){int x=(i*71+13)%int(width),y=top+8+(i*13%std::max(10,horizon-top-12));p.rect(x,y,1+(i&1),1+(i&1),rgb(242,220,183));}p.circle(int(width)*4/5,top+18,7,rgb(247,219,165));}
   else{p.circle(int(width)*4/5,top+19,10,palette.accent);for(int i=0;i<3;++i){int x=(int(width)*i/3+int(animation*4))%int(width);p.round({x-11,top+13+(i&1)*12,38,9},rgb(233,247,238),4);}}
   for(int i=0;i<4;++i){float x=float(int(width)*i/3)-25;screen_polygon(p,{P2{x-45,float(horizon)},P2{x+20,float(horizon-20-(i&1)*13)},P2{x+95,float(horizon)},P2{x+95,float(horizon)}},palette.mountain);}
   constexpr unsigned bands=32;std::array<float,bands+1> depths{};std::array<WorldFrame,bands+1> frames{};
   float nearest=camera.focal*5/std::max(20.f,float(bottom)-camera.horizon)*.52f;for(unsigned i=0;i<=bands;++i){depths[i]=nearest;frames[i]=frame_at(race.track,camera.station+nearest);nearest*=1.145f;}
   // Sort the three opponents once per frame, with a fixed comparison network.
   std::array<unsigned,3> order{0,1,2};auto compare=[&](unsigned a,unsigned b){if(race.traffic[order[a]].distance<race.traffic[order[b]].distance)std::swap(order[a],order[b]);};compare(0,1);compare(1,2);compare(0,1);
   for(int i=int(bands)-1;i>=0;--i){auto &a=frames[unsigned(i)];auto &b=frames[unsigned(i)+1];auto near=camera.point(a,0),far=camera.point(b,0);if(near.z<camera.near||far.z<camera.near)continue;
     float ya=camera.project(near).y,yb=camera.project(far).y;if(ya-yb<.65f||ya<top||yb>bottom)continue;
     bool light=(int(a.center.z/20)&1)==0;
     auto curb=light?rgb(250,229,200):rgb(217,117,104);strip(p,a,b,-6.6f,-6,curb);strip(p,a,b,6,6.6f,curb);strip(p,a,b,-6,6,light?palette.road:palette.road_alt);
     if(light){strip(p,a,b,-2.08f,-1.92f,rgb(224,233,226));strip(p,a,b,1.92f,2.08f,rgb(224,233,226));}
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
