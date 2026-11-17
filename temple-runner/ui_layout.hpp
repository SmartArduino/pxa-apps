// SPDX-License-Identifier: MIT
#pragma once
#include <algorithm>
namespace relic {
struct UiRect {
 float x=0,y=0,w=0,h=0;
 bool contains(float px,float py)const{return px>=x&&py>=y&&px<x+w&&py<y+h;}
 float cx()const{return x+w*.5f;}float cy()const{return y+h*.5f;}
};
inline float ui_pixel_scale(float width,float height){return std::max(.25f,std::min(width/296.f,height/240.f));}
inline float ui_hud_top(float width,float safe_x,float safe_y){return std::max(safe_y,safe_x>width*.18f?safe_x*.35f:0.f);}
// All geometry is in the same viewport units as pointer input. Only the final
// Canvas conversion applies density; borders and fonts share this scale.
struct UiLayout {
 float width,height,u;UiRect record,start,help,quality,pause;
 UiRect paused,paused_heading,resume,sound,home;
 UiRect result,result_heading,score,stats,reason,achievements,rescue,retry;
 UiRect guide,guide_heading;
 UiLayout(float w,float h,float unit,float sx=0,float sy=0):width(w),height(h),u(unit){
  const float cx=w*.5f;
  // Circular displays need the bottom row above the narrowing screen edge.
  const float lift=sx>w*.18f?sx*.55f:0;
  record={cx-98*u,h-132*u-lift,196*u,34*u};
  start={cx-110*u,h-90*u-lift,220*u,40*u};
  const float footer=std::min(268*u,w-2*std::max(10*u,sx)),bw=(footer-12*u)*.5f;
  help={cx-footer*.5f,h-42*u-lift,bw,32*u};quality={help.x+bw+12*u,help.y,bw,32*u};
  pause={w-42*u-sx,8*u+sy,34*u,34*u};
  const float pw=std::min(264*u,w-2*std::max(10*u,sx));
  const float x=cx-pw*.5f;
  paused={x,(h-200*u)*.5f,pw,200*u};
  paused_heading={x+20*u,paused.y+12*u,pw-40*u,36*u};
  resume={x+22*u,paused.y+66*u,pw-44*u,36*u};
  sound={x+22*u,paused.y+110*u,pw-44*u,30*u};
  home={x+22*u,paused.y+150*u,pw-44*u,32*u};
  result={x,(h-212*u)*.5f,pw,212*u};
  result_heading={x+20*u,result.y+10*u,pw-40*u,36*u};
  score={x+20*u,result.y+52*u,pw-40*u,40*u};
  stats={x+22*u,result.y+98*u,pw-44*u,20*u};
  reason={x+22*u,result.y+122*u,pw-44*u,18*u};
  achievements={x+22*u,result.y+144*u,pw-44*u,16*u};
  const float action_width=(pw-48*u)*.5f;
  rescue={x+18*u,result.y+164*u,action_width,32*u};retry={rescue.x+action_width+12*u,rescue.y,action_width,32*u};
  guide={x,(h-220*u)*.5f,pw,220*u};guide_heading={x+20*u,guide.y+10*u,pw-40*u,36*u};
 }
};
}
