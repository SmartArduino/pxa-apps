// SPDX-License-Identifier: MIT
#pragma once
#include <bit>
#include "../common/motion-games/motion.hpp"
#include "../common/motion-games/codec.hpp"
namespace marble {
using motion::Vec;using motion::Random;
struct Maze {
  static constexpr int max_w=15,max_h=15;
  std::array<uint8_t,max_w*max_h> cells{};
  std::array<int,3> stars{};
  std::array<int,max_w*max_h> route{};unsigned route_size=0;
  unsigned collected=0,level=0;int w=9,h=9;
  Vec ball{1.5f,1.5f},velocity{};float elapsed=0;bool won=false;
  int index(int x,int y) const { return y*w+x; }
  bool wall(int x,int y) const {return x<0||y<0||x>=w||y>=h||cells[index(x,y)]==0;}
  void generate(unsigned stage,bool wide,Random& rng,bool tall=false) {
    level=stage;unsigned tier=std::min(2u,stage/4);w=wide?(tier==0?9:tier==1?13:15):7+2*tier;h=wide?(tier==2?9:7):tall?11+2*tier:7+2*tier;cells.fill(0);
    std::array<int,max_w*max_h> stack{};int top=1;stack[0]=index(1,1);cells[stack[0]]=1;
    constexpr int dx[]={0,2,0,-2},dy[]={-2,0,2,0};
    while(top){auto p=stack[top-1];int x=p%w,y=p/w;unsigned dirs[4],n=0;
      for(unsigned d=0;d<4;++d){int xx=x+dx[d],yy=y+dy[d];if(xx>0&&yy>0&&xx<w-1&&yy<h-1&&!cells[index(xx,yy)])dirs[n++]=d;}
      if(!n){--top;continue;}auto d=dirs[rng.below(n)];int xx=x+dx[d],yy=y+dy[d];
      cells[index(x+dx[d]/2,y+dy[d]/2)]=cells[index(xx,yy)]=1;stack[top++]=index(xx,yy);
    }
    // Put all stars on the guaranteed route to the exit, excluding endpoints.
    std::array<int,max_w*max_h> parent,queue{},backward{};parent.fill(-1);
    int first=0,last=1;queue[0]=index(1,1);parent[queue[0]]=queue[0];
    while(first<last){int p=queue[first++];for(int d:{-w,1,w,-1}){int q=p+d;if(q>=0&&q<w*h&&cells[q]&&parent[q]<0){parent[q]=p;queue[last++]=q;}}}
    int n=0,p=index(w-2,h-2);while(p!=index(1,1)){backward[n++]=p;p=parent[p];}
    for(int i=0;i<3;++i)stars[i]=backward[std::clamp((i+1)*n/4,1,n-1)];
    route_size=unsigned(n+1);route[0]=index(1,1);for(int i=0;i<n;++i)route[i+1]=backward[n-1-i];
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
}
namespace marble {
struct Profile {
  static constexpr unsigned size=112;
  unsigned unlocked=0,selected=0,completed=0,sensitivity=1,rotation=0,control=0;
  bool hints=false,flip=false,run=false;unsigned run_level=0,run_w=0,run_h=0,collected=0,elapsed_ms=0;
  float x=1.5f,y=1.5f;std::array<uint32_t,12> best{};
  void snapshot(const Maze& maze){run=!maze.won;run_level=maze.level;run_w=unsigned(maze.w);run_h=unsigned(maze.h);collected=maze.collected;elapsed_ms=uint32_t(std::clamp(maze.elapsed*1000.f,0.f,86400000.f));x=maze.ball.x;y=maze.ball.y;}
  bool restore(Maze& maze) const {
    if(!run||run_level!=maze.level||run_w!=unsigned(maze.w)||run_h!=unsigned(maze.h)||collected>7||!std::isfinite(x)||!std::isfinite(y)||x<1+Maze::radius||y<1+Maze::radius||x>maze.w-1-Maze::radius||y>maze.h-1-Maze::radius)return false;
    for(int yy=int(y-Maze::radius);yy<=int(y+Maze::radius);++yy)for(int xx=int(x-Maze::radius);xx<=int(x+Maze::radius);++xx)if(maze.wall(xx,yy)){
      float dx=x-std::clamp(x,float(xx),float(xx+1)),dy=y-std::clamp(y,float(yy),float(yy+1));if(dx*dx+dy*dy<Maze::radius*Maze::radius-.00001f)return false;}
    maze.ball={x,y};maze.velocity={};maze.collected=collected;maze.elapsed=elapsed_ms*.001f;return true;
  }
  void finish(const Maze& maze){selected=std::min(11u,maze.level+1);completed|=1u<<maze.level;unlocked=std::max(unlocked,std::min(11u,maze.level+1));auto ms=uint32_t(std::clamp(maze.elapsed*1000.f,1.f,86400000.f));if(!best[maze.level]||ms<best[maze.level])best[maze.level]=ms;run=false;}
  std::array<std::byte,size> encode() const {
    std::array<std::byte,size> data{};motion::put32(data.data(),0x4d415a31);
    const unsigned values[]={unlocked,selected,completed,sensitivity,rotation,control,unsigned(hints),unsigned(flip),unsigned(run),run_level,run_w,run_h,collected,elapsed_ms,std::bit_cast<uint32_t>(x),std::bit_cast<uint32_t>(y)};
    for(unsigned i=0;i<16;++i){if(i==2||i>=13)continue;data[4+i]=std::byte(values[i]);}
    motion::put32(data.data()+20,completed);motion::put32(data.data()+24,elapsed_ms);motion::put32(data.data()+28,values[14]);motion::put32(data.data()+32,values[15]);
    for(unsigned i=0;i<12;++i)motion::put32(data.data()+36+i*4,best[i]);
    motion::put32(data.data()+size-4,motion::checksum(std::span(data).first(size-4)));return data;
  }
  bool decode(std::span<const std::byte> data){
    if(data.size()!=size||motion::get32(data.data())!=0x4d415a31||motion::get32(data.data()+size-4)!=motion::checksum(data.first(size-4)))return false;
    Profile p;p.unlocked=unsigned(data[4]);p.selected=unsigned(data[5]);p.sensitivity=unsigned(data[7]);p.rotation=unsigned(data[8]);p.control=unsigned(data[9]);
    if(p.unlocked>11||p.selected>p.unlocked||p.sensitivity>2||p.rotation>3||p.control>2||unsigned(data[10])>1||unsigned(data[11])>1||unsigned(data[12])>1)return false;
    p.hints=bool(data[10]);p.flip=bool(data[11]);p.run=bool(data[12]);p.run_level=unsigned(data[13]);p.run_w=unsigned(data[14]);p.run_h=unsigned(data[15]);p.collected=unsigned(data[16]);
    p.completed=motion::get32(data.data()+20);p.elapsed_ms=motion::get32(data.data()+24);p.x=std::bit_cast<float>(motion::get32(data.data()+28));p.y=std::bit_cast<float>(motion::get32(data.data()+32));
    if(!std::isfinite(p.x)||!std::isfinite(p.y)||p.x<0||p.y<0||p.x>15||p.y>15)return false;
    if(p.completed>4095||p.elapsed_ms>86400000||p.collected>7||p.run_level>11||(p.run&&(p.run_level>p.unlocked||p.run_w<7||p.run_w>15||p.run_h<7||p.run_h>15)))return false;
    for(unsigned i=0;i<12;++i){p.best[i]=motion::get32(data.data()+36+i*4);if(p.best[i]>86400000)return false;}
    if(data[6]!=std::byte{})return false;for(unsigned i=17;i<20;++i)if(data[i]!=std::byte{})return false;for(unsigned i=84;i<size-4;++i)if(data[i]!=std::byte{})return false;
    *this=p;return true;
  }
};
}
