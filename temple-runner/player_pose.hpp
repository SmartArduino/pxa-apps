// SPDX-License-Identifier: MIT
#pragma once
#include "core.hpp"
#include <algorithm>
#include <cmath>
namespace relic {
struct V3 {float x,y,z;};
namespace motion {
constexpr float pi=3.14159265f;
inline V3 add(V3 a,V3 b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
inline V3 sub(V3 a,V3 b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
inline V3 mul(V3 a,float k){return {a.x*k,a.y*k,a.z*k};}
inline float dot(V3 a,V3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
inline float length(V3 a){return std::sqrt(dot(a,a));}
inline V3 unit(V3 a){return mul(a,1/std::max(.0001f,length(a)));}
inline float smooth(float a,float b,float x){x=std::clamp((x-a)/(b-a),0.f,1.f);return x*x*(3-2*x);}
inline float mix(float a,float b,float t){return a+(b-a)*t;}
inline V3 mix(V3 a,V3 b,float t){return add(a,mul(sub(b,a),t));}
// Two-bone IK keeps thigh/shin and upper/forearm lengths constant. The bend
// direction is projected into the plane perpendicular to the target chord.
inline V3 joint(V3 a,V3 b,float upper,float lower,V3 bend){
 V3 delta=sub(b,a);float d=std::clamp(length(delta),.001f,upper+lower-.0001f);V3 axis=unit(delta);
 float along=(upper*upper-lower*lower+d*d)/(2*d);
 V3 tangent=unit(sub(bend,mul(axis,dot(bend,axis))));
 return add(add(a,mul(axis,along)),mul(tangent,std::sqrt(std::max(0.f,upper*upper-along*along))));
}
struct Pose {
 V3 hip[2],knee[2],ankle[2],shoulder[2],elbow[2],wrist[2];
 float foot_pitch[2]{};
 float phase=0,hip_height=.88f,pitch=.07f,yaw=0,roll=0,lift=0,slide=0,jump=0;
};
inline Pose pose(const State& s,float title_time){
 Pose p;const bool idle=s.mode==Mode::Title||s.mode==Mode::Help;
 p.phase=idle?0:s.elapsed*7.2f+s.distance*.42f;
 p.slide=smooth(0,1,s.pose_slide);
 float flight=s.jump_t>0?1-s.jump_t/.94f:0;
 float tuck=std::sin(pi*flight);p.jump=s.jump_t>0?tuck*tuck:0;
 float landing=s.landing>0?std::sin(pi*s.landing/.18f)*.075f:0;
 float bob=idle?std::sin(title_time*2)*.012f:(1-p.slide)*(1-p.jump)*(.022f-.022f*std::cos(p.phase*2));
 p.hip_height=.88f+bob-.51f*p.slide-landing;
 p.pitch=mix(.15f,1.08f,p.slide)+p.jump*.09f+landing*.6f;
 p.yaw=std::clamp((s.lane*1.65f-s.lane_x)*.15f,-.2f,.2f)+s.turn_lean*.55f+p.slide*.12f;
 p.roll=(1-p.slide)*(-std::sin(p.phase)*.028f-std::clamp((s.lane*1.65f-s.lane_x)*.12f,-.13f,.13f)-s.turn_lean*.20f)+p.slide*.07f;
 p.lift=s.jump;
 for(int i=0;i<2;i++){
  float side=i?1.f:-1.f,cycle=std::fmod(p.phase/(2*pi)+(i?.5f:0),1.f);
  float stance=std::min(1.f,cycle/.20f),swing=std::clamp((cycle-.20f)/.80f,0.f,1.f);
  float foot_z=cycle<.20f?mix(.40f,-.39f,stance):mix(-.39f,.40f,smooth(0,1,swing));
  float foot_y=.13f+(cycle<.20f?0:std::sin(pi*swing)*.28f);
  if(idle){foot_z=i?.035f:-.035f;foot_y=.13f;}
  V3 ankle={side*.16f,foot_y,foot_z};
  ankle=mix(ankle,{side*.17f,.36f+(i?.035f:0),-.14f},p.jump);
  ankle=mix(ankle,{side*(i?.21f:.26f),i?.20f:.13f,i?.66f:-.24f},p.slide);
  float toe=idle?0:std::sin(2*pi*cycle)*.16f;
  p.foot_pitch[i]=mix(toe*(1-p.jump),i?-.28f:.14f,p.slide);
  ankle.y=std::max(ankle.y,.13f+.25f*std::abs(std::sin(p.foot_pitch[i])));
  p.hip[i]={side*.145f,p.hip_height,0};p.ankle[i]=ankle;
  p.knee[i]=joint(p.hip[i],ankle,.45f,.44f,mix(V3{0,0,1},V3{0,1,.15f},p.slide));
  float wave=idle?0:-std::cos(p.phase+(i?pi:0));
  p.shoulder[i]={side*.28f,1.43f,.025f};
  V3 wrist={side*.34f,1.13f+wave*.08f,.05f+wave*.26f};
  if(idle)wrist={side*.34f,1.02f,.04f};
  wrist=mix(wrist,{side*.31f,1.57f,.25f},p.jump);
  // One arm braces low while the other guards the head; avoid a symmetric squat.
  wrist=mix(wrist,i?V3{.31f,1.41f,.28f}:V3{-.43f,1.13f,.36f},p.slide);
  wrist.z-=side*s.turn_lean*.055f*(1-p.slide);
  p.wrist[i]=wrist;p.elbow[i]=joint(p.shoulder[i],wrist,.30f,.27f,{side*.2f,-.1f,-1});
 }
 return p;
}
}
}
