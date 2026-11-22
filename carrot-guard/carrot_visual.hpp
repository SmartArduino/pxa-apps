// SPDX-License-Identifier: MIT
#pragma once
#include <algorithm>
#include <cmath>

namespace carrot {
struct CarrotVisual {
  float tapped=0,hit=0;
  int previousHearts=10;
  void reset(int hearts){tapped=hit=0;previousHearts=hearts;}
  void tick(float dt,int hearts){
    tapped=std::max(0.f,tapped-dt);hit=std::max(0.f,hit-dt);
    if(hearts<previousHearts){hit=.48f;tapped=0;}
    previousHearts=hearts;
  }
  void tap(){if(hit<=0)tapped=.62f;}
  static int damageStage(int hearts,int maximum){
    if(hearts>=maximum)return 0;
    if(hearts*10>=maximum*7)return 1;
    return hearts*10>=maximum*3?2:3;
  }
  int pose(unsigned milliseconds)const{
    if(hit>0)return 2;
    if(tapped>0)return 3;
    return milliseconds%3600>=3380?1:0;
  }
  float jump()const{return tapped>0?std::sin((.62f-tapped)/.62f*3.14159265f)*8:0;}
  float shake()const{return hit>0?std::sin(hit*65)*(hit/.48f)*3:0;}
};
}
