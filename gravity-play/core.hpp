// SPDX-License-Identifier: MIT
#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
namespace gravity {
struct Vec { float x=0,y=0; };
struct Random {
  uint32_t state=0x9e3779b9;
  uint32_t next() { auto x=state?state:0x9e3779b9u; x^=x<<13;x^=x>>17;x^=x<<5;return state=x; }
  unsigned below(unsigned n) { uint32_t x,threshold=uint32_t(-n)%n;do{x=next();}while(x<threshold);return x%n; }
};
struct Maze {
  static constexpr int max_w=15,max_h=15;
  std::array<uint8_t,max_w*max_h> cells{};
  std::array<int,3> stars{};
  unsigned collected=0,level=0;int w=9,h=9;
  Vec ball{1.5f,1.5f},velocity{};float elapsed=0;bool won=false;
  int index(int x,int y) const { return y*w+x; }
  bool wall(int x,int y) const {return x<0||y<0||x>=w||y>=h||cells[index(x,y)]==0;}
  void generate(unsigned stage,bool wide,Random& rng,bool tall=false) {
    level=stage;w=wide?13:9;h=wide?7:tall?13:9;cells.fill(0);
    std::array<int,max_w*max_h> stack{};int top=1;stack[0]=index(1,1);cells[stack[0]]=1;
    constexpr int dx[]={0,2,0,-2},dy[]={-2,0,2,0};
    while(top){auto p=stack[top-1];int x=p%w,y=p/w;unsigned dirs[4],n=0;
      for(unsigned d=0;d<4;++d){int xx=x+dx[d],yy=y+dy[d];if(xx>0&&yy>0&&xx<w-1&&yy<h-1&&!cells[index(xx,yy)])dirs[n++]=d;}
      if(!n){--top;continue;}auto d=dirs[rng.below(n)];int xx=x+dx[d],yy=y+dy[d];
      cells[index(x+dx[d]/2,y+dy[d]/2)]=cells[index(xx,yy)]=1;stack[top++]=index(xx,yy);
    }
    // Put all stars on the guaranteed route to the exit, excluding endpoints.
    std::array<int,max_w*max_h> parent,queue{},route{};parent.fill(-1);
    int first=0,last=1;queue[0]=index(1,1);parent[queue[0]]=queue[0];
    while(first<last){int p=queue[first++];for(int d:{-w,1,w,-1}){int q=p+d;if(q>=0&&q<w*h&&cells[q]&&parent[q]<0){parent[q]=p;queue[last++]=q;}}}
    int n=0,p=index(w-2,h-2);while(p!=index(1,1)){route[n++]=p;p=parent[p];}
    for(int i=0;i<3;++i)stars[i]=route[std::clamp((i+1)*n/4,1,n-1)];
    ball={1.5f,1.5f};velocity={};collected=0;elapsed=0;won=false;
  }
  static constexpr float radius=.27f;
  void move_axis(float delta,bool horizontal) {
    float &axis=horizontal?ball.x:ball.y;const float previous=axis;axis+=delta;
    for(int y=int(ball.y-radius);y<=int(ball.y+radius);++y)
      for(int x=int(ball.x-radius);x<=int(ball.x+radius);++x)if(wall(x,y)){
        // Bounded substeps are < .032 cells. Reject just the colliding axis;
        // the other axis can slide along the wall, including rounded corners.
        float dx=ball.x-std::clamp(ball.x,float(x),float(x+1));
        float dy=ball.y-std::clamp(ball.y,float(y),float(y+1));
        if(dx*dx+dy*dy<radius*radius){axis=previous;(horizontal?velocity.x:velocity.y)*=.2f;return;}
      }
  }
  void tick(float dt,Vec tilt,float sensitivity=1) {
    if(won||!(dt>0)||!std::isfinite(tilt.x)||!std::isfinite(tilt.y))return;
    dt=std::min(dt,.05f);elapsed+=dt;
    unsigned steps=unsigned(std::ceil(dt/.008f));dt/=steps;
    for(unsigned i=0;i<steps;++i){
      float drag=std::exp(-2.9f*dt),force=16.f*sensitivity;
      velocity.x=std::clamp((velocity.x+tilt.x*force*dt)*drag,-4.f,4.f);
      velocity.y=std::clamp((velocity.y+tilt.y*force*dt)*drag,-4.f,4.f);
      move_axis(velocity.x*dt,true);move_axis(velocity.y*dt,false);
      for(unsigned j=0;j<3;++j){float dx=ball.x-(stars[j]%w+.5f),dy=ball.y-(stars[j]/w+.5f);if(dx*dx+dy*dy<.23f)collected|=1u<<j;}
      float dx=ball.x-(w-1.5f),dy=ball.y-(h-1.5f);
      if(collected==7&&dx*dx+dy*dy<.16f){won=true;velocity={};return;}
    }
  }
};
struct Motion {
  Vec filtered{},zero{},sum{};float calibration_error=0;
  unsigned samples=0,calibration_count=0,rotation=0;bool calibrating=false,calibrated=false;
  uint64_t last=0,shake_at=0,previous_hit=0;unsigned hits=0;
  std::array<float,3> raw{0,0,1};
  Vec orient(Vec v) const {for(unsigned i=0;i<rotation%4;++i)v={-v.y,v.x};return v;}
  void calibrate(){sum={};calibration_error=0;calibration_count=0;calibrating=true;calibrated=false;hits=0;}
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
      if(magnitude<.5f||magnitude>1.7f||jerk>.012f){sum={};calibration_count=0;calibration_error=0;}
      else {sum.x+=reading.x;sum.y+=reading.y;++calibration_count;
        if(calibration_count>=16){zero={sum.x/calibration_count,sum.y/calibration_count};calibrating=false;calibrated=true;}}
    }
    return shake;
  }
  Vec tilt() const {auto v=orient({filtered.x-zero.x,filtered.y-zero.y});
    auto clean=[](float x){return std::abs(x)<.035f?0.f:std::clamp((x-std::copysign(.035f,x))*2.2f,-1.f,1.f);};return {clean(v.x),clean(v.y)};}
  bool fresh(uint64_t now) const {return last&&now>=last&&now-last<=400000;}
  void suspend(){last=0;samples=0;hits=0;previous_hit=0;}
};
}
