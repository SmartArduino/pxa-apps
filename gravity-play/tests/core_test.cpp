// SPDX-License-Identifier: MIT
#include "../core.hpp"
#include <cassert>
#include <cstdio>
#include <limits>
using namespace gravity;
bool overlaps(const Maze& m){
  for(int y=0;y<m.h;++y)for(int x=0;x<m.w;++x)if(m.wall(x,y)){
    float dx=m.ball.x-std::clamp(m.ball.x,float(x),float(x+1)),dy=m.ball.y-std::clamp(m.ball.y,float(y),float(y+1));
    if(dx*dx+dy*dy<Maze::radius*Maze::radius-.00001f)return true;
  }return false;
}
void route_test(unsigned seed,bool wide,bool tall=false){
  Random rng{seed};Maze m;m.generate(seed%12,wide,rng,tall);
  std::array<int,225> parent{},queue{},path{};parent.fill(-1);int first=0,last=1,start=m.index(1,1),end=m.index(m.w-2,m.h-2);queue[0]=start;parent[start]=start;
  while(first<last){int p=queue[first++];for(int d:{-m.w,1,m.w,-1}){int q=p+d;if(q>=0&&q<m.w*m.h&&m.cells[q]&&parent[q]<0){parent[q]=p;queue[last++]=q;}}}
  assert(parent[end]>=0);for(auto star:m.stars)assert(parent[star]>=0&&star!=start&&star!=end);
  assert(m.stars[0]!=m.stars[1]&&m.stars[1]!=m.stars[2]&&m.stars[0]!=m.stars[2]);
  int n=0,p=end;while(p!=start){path[n++]=p;p=parent[p];}
  for(auto star:m.stars)assert(std::find(path.begin(),path.begin()+n,star)!=path.begin()+n);
  if(seed<=30){
    for(int j=n-1;j>=0;--j){int target=path[j];Vec goal{target%m.w+.5f,target/m.w+.5f};unsigned step=0;
      for(;step<1200;++step){Vec delta{goal.x-m.ball.x,goal.y-m.ball.y};if(delta.x*delta.x+delta.y*delta.y<.006f)break;
        m.tick(.016f,{std::clamp(delta.x*1.4f-m.velocity.x*.65f,-1.f,1.f),std::clamp(delta.y*1.4f-m.velocity.y*.65f,-1.f,1.f)});
        assert(!overlaps(m));if(m.won)break;
      }assert(step<1200);
    }assert(m.won&&m.collected==7);
  }
  m.generate(0,wide,rng,tall);
  for(int i=0;i<2000;++i){Vec tilt{float(int(rng.below(201))-100)/100,float(int(rng.below(201))-100)/100};m.tick(.05f,tilt,1.45f);if(overlaps(m)){std::fprintf(stderr,"overlap seed=%u wide=%d step=%d pos=%f,%f vel=%f,%f tilt=%f,%f\n",seed,wide,i,m.ball.x,m.ball.y,m.velocity.x,m.velocity.y,tilt.x,tilt.y);std::abort();}assert(std::isfinite(m.ball.x)&&std::isfinite(m.ball.y));}
  auto before=m.ball;float time=m.elapsed;m.tick(0,{1,1});assert(m.ball.x==before.x&&m.elapsed==time);
  m.tick(1,{std::numeric_limits<float>::quiet_NaN(),0});assert(m.ball.x==before.x&&m.elapsed==time);
}
int main(){
  for(unsigned seed=1;seed<=500;++seed){route_test(seed,false);route_test(seed,true);route_test(seed,false,true);}
  Motion motion;motion.calibrate();uint64_t time=1000000;
  for(unsigned i=0;i<30;++i){time+=50000;assert(!motion.acceleration({0,0,9806650},time));}
  assert(motion.calibrated&&!motion.calibrating&&motion.fresh(time));assert(!motion.fresh(time+500000));assert(motion.tilt().x==0&&motion.tilt().y==0);
  for(unsigned i=0;i<30;++i){time+=50000;assert(!motion.acceleration({0,-2451662,9498000},time));}
  assert(motion.tilt().x>.4f);motion.rotation=1;assert(motion.tilt().y>.4f);
  motion.suspend();assert(!motion.fresh(time));time+=2000000;
  assert(!motion.acceleration({0,0,9806650},time));
  assert(!motion.acceleration({20000000,0,9806650},time+=150000));
  assert(motion.acceleration({-20000000,0,9806650},time+=150000));
  assert(!motion.acceleration({20000000,0,9806650},time+=150000));
  Random r;std::array<unsigned,6> bins{};for(unsigned i=0;i<60000;++i)++bins[r.below(6)];for(auto n:bins)assert(n>9000&&n<11000);
  std::puts("1500 mazes reachable; 90 solved by physics; 3M collision steps; calibration, stale samples, shake debounce and dice distribution passed");
}
