// SPDX-License-Identifier: MIT
#pragma once
#include "../common/motion-games/paint.hpp"
namespace fortune {
// Turned wooden cylinder, an open elliptical mouth and thin inscribed bamboo
// slips. The body, opening and cord are deliberately unlike a paper carton.
inline void draw_sign_cup(motion::Paint& p,motion::Rect a,bool drawing,float progress,float energy){
 using namespace motion;
 int cx=a.x+a.w/2,base=a.y+a.h-7,ch=std::min(132,std::max(30,a.h*55/100)),cw=std::min(98,std::max(36,ch*3/4));float rx=cw*.5f,lip=std::max(4.f,cw*.105f);
 p.ellipse(cx,a.y+a.h/2,std::min(a.w/2-4,94),std::min(a.h/2-3,82),rgb(247,237,217));p.ellipse(cx,base+5,cw/2+9,4,rgb(220,209,185));
 float angle=drawing?std::sin(progress*32)*(1-progress)*(.105f+energy*.025f):0,cs=std::cos(angle),sn=std::sin(angle);
 auto at=[&](float x,float y){return Point{cx+x*cs-y*sn,base+x*sn+y*cs};};
 auto quad=[&](float x0,float y0,float x1,float y1,float x2,float y2,float x3,float y3,uint16_t c){p.quad({at(x0,y0),at(x1,y1),at(x2,y2),at(x3,y3)},c);};
 auto oval=[&](float x,float y,float w,float h,uint16_t c,bool front=false){unsigned steps=front?6:12;for(unsigned i=0;i<steps;i+=2){float t=i*.5235988f,u=(i+1)*.5235988f,v=(i+2)*.5235988f;p.quad({at(x,y),at(x+std::cos(t)*w,y+std::sin(t)*h),at(x+std::cos(u)*w,y+std::sin(u)*h),at(x+std::cos(v)*w,y+std::sin(v)*h)},c);}};
 // Cylindrical staves and rounded foot, rather than a flat-bottomed box.
 quad(-rx,-ch,rx,-ch,rx*.90f,-4,-rx*.90f,-4,rgb(142,99,62));oval(0,-4,rx*.90f,lip,rgb(130,88,54));
 constexpr uint16_t wood[]={rgb(133,91,58),rgb(160,116,72),rgb(178,132,82),rgb(166,119,71),rgb(144,98,57)};
 for(unsigned i=0;i<5;++i){float x=-rx+i*cw/5.f,z=-rx+(i+1)*cw/5.f;quad(x,-ch,z,-ch,z*.90f,-4,x*.90f,-4,wood[i]);}
 for(unsigned i=0;i<4;++i){float x=-rx*.72f+i*rx*.45f;p.line(at(x,-ch+lip+11),at(x*.91f,-ch*.24f),1,rgb(190,146,95));}
 oval(0,-ch,rx+2,lip+2,rgb(113,76,47));oval(0,-ch,rx-2,lip-1,rgb(62,49,37));
 int visible=std::max(14,std::min(ch,a.h-ch-12));
 for(unsigned i=0;i<7;++i){float x=(int(i)-3)*cw*.105f,lean=(int(i)-3)*1.8f,height=visible*(.74f+float(i*7%5)*.055f),lift=drawing&&i==3?std::max(0.f,(progress-.54f)/.46f)*visible*.85f:0;
  float top=std::max(float(a.y+5-base),-ch-height-lift),tip=x+lean,half=std::max(1.1f,cw*.025f);quad(x-half,-ch+lip*.5f,x+half,-ch+lip*.5f,tip+half,top,tip-half,top,rgb(228+unsigned(i%2)*6,211,174));
  p.line(at(tip+half,top+2),at(x+half,-ch+2),.7f,rgb(177,149,109));p.line(at(tip-half+.4f,top+6),at(tip+half-.4f,top+6),1,rgb(165,65,52));
  for(unsigned j=0;j<i%3+1;++j)p.line(at(tip-.7f,top+10+j*3),at(tip+.8f,top+10+j*3),.75f,rgb(121,101,75));
 }
 // The front half of the rim hides the lower ends of the inserted slips.
 oval(0,-ch,rx+2,lip+2,rgb(186,139,85),true);oval(0,-ch+2,rx,lip*.45f,rgb(206,167,108),true);
 for(float y:{-ch*.78f,-12.f}){quad(-rx*.96f,y,rx*.96f,y,rx*.96f,y+5,-rx*.96f,y+5,rgb(63,101,88));quad(-rx*.96f,y,rx*.96f,y,rx*.96f,y+1,-rx*.96f,y+1,rgb(222,190,125));}
 // Vermilion seal and a tied tassel give the silhouette a clear Chinese cue.
 auto seal=at(0,-ch*.44f);int rr=std::max(7,std::min(16,ch/6));p.circle(int(seal.x),int(seal.y),rr,rgb(172,69,53));p.circle(int(seal.x),int(seal.y),rr-2,rgb(188,80,60));
 if(!drawing)p.label({int(seal.x)-rr,int(seal.y)-rr,rr*2,rr*2},"签",rgb(255,238,194),1,rr+2);
 float knot_x=rx*.91f,knot_y=-ch*.62f;p.line(at(knot_x,knot_y-6),at(knot_x+9,knot_y+3),1.5f,rgb(161,64,52));p.line(at(knot_x+9,knot_y+3),at(knot_x+8,knot_y+14),1.5f,rgb(161,64,52));auto bead=at(knot_x+8,knot_y+14);p.circle(int(bead.x),int(bead.y),2,rgb(235,194,112));
 for(int i=-1;i<=1;++i)p.line(at(knot_x+8+i,knot_y+17),at(knot_x+8+i*2,knot_y+std::max(22.f,std::min(32.f,ch*.4f))),1,rgb(176,68,54));
 if(drawing)for(int i=0;i<3;++i){float t=progress*5+i;int x=cx+int(std::sin(t)*cw*.78f),y=a.y+6+(i+1)*a.h/6;p.star(x,y,2,i&1?rgb(218,170,84):rgb(190,102,73));}
}
}
