// SPDX-License-Identifier: MIT
#pragma once
#include "../common/motion-games/paint.hpp"
namespace dice {
struct V3 {float x,y,z;};
struct Matrix {
 std::array<float,9> m;
 static Matrix angles(float ax,float ay,float az){float cx=std::cos(ax),sx=std::sin(ax),cy=std::cos(ay),sy=std::sin(ay),cz=std::cos(az),sz=std::sin(az);
  return {{{cz*cy,cz*sy*sx-sz*cx,cz*sy*cx+sz*sx,sz*cy,sz*sy*sx+cz*cx,sz*sy*cx-cz*sx,-sy,cy*sx,cy*cx}}};}
 V3 apply(V3 v)const{return {m[0]*v.x+m[1]*v.y+m[2]*v.z,m[3]*v.x+m[4]*v.y+m[5]*v.z,m[6]*v.x+m[7]*v.y+m[8]*v.z};}
};
inline V3 rotated(V3 v,float x,float y,float z){return Matrix::angles(x,y,z).apply(v);}
struct Pose {float x=0,y=0,z=0;};
inline Pose face_pose(unsigned value){constexpr float half=1.570796327f;switch(value){case 2:return {0,-half,0};case 3:return {half,0,0};case 4:return {-half,0,0};case 5:return {0,half,0};case 6:return {0,2*half,0};default:return {};}}
inline V3 plus(V3 a,V3 b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
inline V3 times(V3 a,float b){return {a.x*b,a.y*b,a.z*b};}
inline void cube(motion::Paint& p,motion::Rect box,unsigned value,float remaining,unsigned variant,float energy=0){
 using namespace motion;auto target=face_pose(value);float spin=remaining*remaining;
 Pose pose{target.x+12.56637f*spin,target.y+18.849556f*spin,target.z+(6.283185f+variant*.2f)*spin};
 struct Face {V3 n,u,v;unsigned value;float depth;};
 std::array<Face,6> faces{{{{0,0,1},{1,0,0},{0,1,0},1,0},{{1,0,0},{0,0,-1},{0,1,0},2,0},{{0,1,0},{1,0,0},{0,0,-1},3,0},{{0,-1,0},{1,0,0},{0,0,1},4,0},{{-1,0,0},{0,0,1},{0,1,0},5,0},{{0,0,-1},{-1,0,0},{0,1,0},6,0}}};
 auto matrix=Matrix::angles(pose.x,pose.y,pose.z);auto rotate=[&](V3 v){return matrix.apply(v);};
 float extent=1;for(int i=0;i<8;++i){auto v=rotate({i&1?1.f:-1.f,i&2?1.f:-1.f,i&4?1.f:-1.f});extent=std::max({extent,std::abs(v.x),std::abs(v.y)});}
 float size=std::min(box.w,box.h)*.43f/extent;
 float bounce=std::abs(std::sin((1-remaining)*19+variant))*(3+energy*2)*remaining;
 Point center{box.x+box.w*.5f+std::sin((1-remaining)*24+variant)*remaining*2,box.y+box.h*.5f-bounce};
 p.ellipse(int(center.x),box.y+box.h-5,std::max(3,int(size*.8f)),std::max(2,int(size*.16f)),rgb(217,197,176));
 auto point=[&](V3 v){auto a=rotate(v);return Point{center.x+a.x*size,center.y-a.y*size};};
 for(auto &f:faces)f.depth=rotate(f.n).z;
 std::sort(faces.begin(),faces.end(),[](const Face&a,const Face&b){return a.depth<b.depth;});
 for(auto &f:faces){if(f.depth<=.001f)continue;
  unsigned shade=unsigned(208+47*std::clamp(f.depth,0.f,1.f));uint16_t color=variant%3==1?rgb(shade,shade*96/100,shade):variant%3==2?rgb(shade,shade,shade*94/100):rgb(shade,shade,shade);
  auto at=[&](float x,float y){return plus(f.n,plus(times(f.u,x),times(f.v,y)));};
  p.quad({point(at(-1,-1)),point(at(1,-1)),point(at(1,1)),point(at(-1,1))},variant%3==1?rgb(shade-15,(shade-15)*96/100,shade-15):rgb(shade-15,shade-15,shade-15));
  p.quad({point(at(-.94f,-.94f)),point(at(.94f,-.94f)),point(at(.94f,.94f)),point(at(-.94f,.94f))},color);
  auto dot=[&](float x,float y){static constexpr float cs[]={1,.7071068f,0,-.7071068f,-1,-.7071068f,0,.7071068f},sn[]={0,.7071068f,1,.7071068f,0,-.7071068f,-1,-.7071068f};std::array<Point,8> edge;for(unsigned i=0;i<8;++i)edge[i]=point(at(x+cs[i]*.115f,y+sn[i]*.115f));
   uint16_t ink_color=f.value==1?coral:ink;for(unsigned i=1;i<7;i+=2)p.quad({edge[0],edge[i],edge[i+1],edge[i+2]},ink_color);};
  if(f.value&1)dot(0,0);if(f.value>=2){dot(-.48f,-.48f);dot(.48f,.48f);}if(f.value>=4){dot(.48f,-.48f);dot(-.48f,.48f);}if(f.value==6){dot(-.48f,0);dot(.48f,0);}
 }
}
}
