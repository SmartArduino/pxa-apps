// SPDX-License-Identifier: MIT
#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
namespace motion {
struct Vec { float x=0,y=0; };
struct Random {
  uint32_t state=0x9e3779b9;
  uint32_t next() { auto x=state?state:0x9e3779b9u; x^=x<<13;x^=x>>17;x^=x<<5;return state=x; }
  unsigned below(unsigned n) { uint32_t x,threshold=uint32_t(-n)%n;do{x=next();}while(x<threshold);return x%n; }
};
struct Motion {
  Vec filtered{},zero{},sum{},variance{};uint64_t calibration_start=0;bool flip_x=false;
  unsigned samples=0,calibration_count=0,rotation=0;bool calibrating=false,calibrated=false;
  uint64_t last=0,shake_at=0,previous_hit=0;unsigned hits=0;
  std::array<float,3> raw{0,0,1};
  Vec orient(Vec v) const {for(unsigned i=0;i<rotation%4;++i)v={-v.y,v.x};if(flip_x)v.x=-v.x;return v;}
  void calibrate(){sum={};variance={};calibration_start=0;calibration_count=0;calibrating=true;calibrated=false;hits=0;}
  bool acceleration(std::array<int32_t,3> value,uint64_t now) {
    std::array<float,3> next;float magnitude=0,jerk=0;
    for(unsigned i=0;i<3;++i){next[i]=value[i]/9806650.f;magnitude+=next[i]*next[i];jerk+=std::pow(next[i]-raw[i],2);}
    // Gen4's mounted IMU axes are rotated relative to the portrait screen.
    Vec reading{-next[1],next[0]};filtered=samples?Vec{filtered.x*.65f+reading.x*.35f,filtered.y*.65f+reading.y*.35f}:reading;
    bool shake=false;
    if(last&&now>last&&now-last<300000&&jerk>.24f&&magnitude>1.3f&&now>previous_hit+100000){
      hits=(now-previous_hit<650000)?hits+1:1;previous_hit=now;
      if(hits>=2&&now>shake_at+1700000){shake_at=now;hits=0;shake=true;}
    }
    raw=next;last=now;++samples;
    if(calibrating){
      if(magnitude<.5f||magnitude>1.7f||jerk>.012f){sum={};variance={};calibration_start=0;calibration_count=0;}
      else {if(!calibration_count)calibration_start=now;++calibration_count;
        Vec delta{reading.x-sum.x,reading.y-sum.y};sum.x+=delta.x/calibration_count;sum.y+=delta.y/calibration_count;
        variance.x+=delta.x*(reading.x-sum.x);variance.y+=delta.y*(reading.y-sum.y);
        if(calibration_count>=16&&now-calibration_start>=750000){
          if(variance.x/calibration_count>.000625f||variance.y/calibration_count>.000625f){sum={};variance={};calibration_count=0;}
          else {zero=sum;calibrating=false;calibrated=true;}}}
    }
    return shake;
  }
  Vec tilt() const {auto v=orient({filtered.x-zero.x,filtered.y-zero.y});
    auto clean=[](float x){return std::abs(x)<.035f?0.f:std::clamp((x-std::copysign(.035f,x))*2.2f,-1.f,1.f);};return {clean(v.x),clean(v.y)};}
  bool fresh(uint64_t now) const {return last&&now>=last&&now-last<=400000;}
  void suspend(){last=0;samples=0;hits=0;previous_hit=0;}
};
}
