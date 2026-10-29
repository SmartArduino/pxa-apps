// SPDX-License-Identifier: MIT
#pragma once
#include "model.hpp"
#include <span>
namespace racer {
struct V3 {float x=0,y=0,z=0;};
struct P2 {float x=0,y=0;};
struct WorldFrame {V3 center;float nx=1,nz=0,slope=0;};
inline WorldFrame frame_at(const Track& track,float station){auto c=track.at(station);float t=track.tangent(station),inv=1/std::sqrt(1+t*t);return {{c.x,c.y,station},inv,-t*inv,(track.at(station+5).y-track.at(station-5).y)*.1f};}
struct Camera {
 float focal=180,horizon=80,mid=120,station=0,near=.7f;V3 position{};float cs=1,sn=0,follow=12;
 void setup(const Race& race,int width,int top,int bottom){
   focal=std::min(width*.88f,float(bottom-top)*1.4f);horizon=top+(bottom-top)*.28f;mid=width*.5f;
   float nearest=focal*5/std::max(20.f,bottom-horizon);follow=nearest*1.45f+2;
   station=race.distance-follow;auto frame=frame_at(race.track,station);cs=frame.nx;sn=-frame.nz;
   position={frame.center.x+frame.nx*race.x*2.28f,frame.center.y+5,station+frame.nz*race.x*2.28f};
   horizon+=std::clamp(frame.slope*focal,-20.f,20.f);
 }
 V3 view(V3 v)const{float x=v.x-position.x,z=v.z-position.z;return {x*cs-z*sn,v.y-position.y,x*sn+z*cs};}
 V3 point(const WorldFrame& frame,float lateral,float height=0,float ahead=0)const{
   return view({frame.center.x+frame.nx*lateral-frame.nz*ahead,frame.center.y+height+frame.slope*ahead,frame.center.z+frame.nz*lateral+frame.nx*ahead});}
 P2 project(V3 p)const{return {mid+focal*p.x/p.z,horizon-focal*p.y/p.z};}
};
// Clip before Q4 conversion: near-plane crossings and off-screen bends cannot
// overflow GameRender coordinates or invalidate the whole command list.
inline unsigned clip_near(std::span<const V3> input,std::array<V3,12>& out,float plane=.7f){
 unsigned n=0;if(input.empty())return 0;auto a=input.back();for(auto b:input){bool ai=a.z>=plane,bi=b.z>=plane;
   if(ai!=bi){float t=(plane-a.z)/(b.z-a.z);out[n++]={a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t,plane};}if(bi)out[n++]=b;a=b;}return n;
}
struct ClipRect {float left=0,top=0,right=240,bottom=284;};
inline unsigned clip_screen(std::span<const P2> input,ClipRect r,std::array<P2,12>& output){
 std::array<P2,12> a{},b{};unsigned count=unsigned(input.size());if(count>8)return 0;
 // Most projected faces are wholly visible or wholly outside one edge.
 // Avoid four rounds of clipping and scratch-array copies for those faces.
 unsigned any=0,all=15;for(auto p:input){unsigned code=(p.x<r.left?1u:0u)|(p.x>r.right?2u:0u)|(p.y<r.top?4u:0u)|(p.y>r.bottom?8u:0u);any|=code;all&=code;}
 if(all)return 0;if(!any){std::copy(input.begin(),input.end(),output.begin());return count;}
 std::copy(input.begin(),input.end(),a.begin());
 for(unsigned side=0;side<4&&count;++side){unsigned n=0;auto coord=[&](P2 p){return side<2?p.x:p.y;};float edge=side==0?r.left:side==1?r.right:side==2?r.top:r.bottom;
   auto inside=[&](P2 p){return side==0||side==2?coord(p)>=edge:coord(p)<=edge;};auto previous=a[count-1];
   for(unsigned i=0;i<count;++i){auto current=a[i];bool ai=inside(previous),bi=inside(current);if(ai!=bi){float t=(edge-coord(previous))/(coord(current)-coord(previous));b[n++]={previous.x+(current.x-previous.x)*t,previous.y+(current.y-previous.y)*t};}if(bi)b[n++]=current;previous=current;}
   count=n;a=b;
 }output=a;return count;
}
}
