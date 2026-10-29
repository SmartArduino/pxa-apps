// SPDX-License-Identifier: MIT
#include "../../../shake-dice/model.hpp"
#include "../../../marble-maze/model.hpp"
#include <cassert>
#include <cstdio>
#include <limits>
using motion::Random;using marble::Maze;
bool overlaps(const Maze& m){for(int y=0;y<m.h;++y)for(int x=0;x<m.w;++x)if(m.wall(x,y)){float dx=m.ball.x-std::clamp(m.ball.x,float(x),float(x+1)),dy=m.ball.y-std::clamp(m.ball.y,float(y),float(y+1));if(dx*dx+dy*dy<Maze::radius*Maze::radius-.00001f)return true;}return false;}
void maze_test(unsigned seed,bool wide,bool tall){Random rng{seed};Maze m;m.generate(seed%12,wide,rng,tall);
 std::array<int,225> parent{},queue{},path{};parent.fill(-1);int begin=0,end=1,start=m.index(1,1),goal=m.index(m.w-2,m.h-2);queue[0]=start;parent[start]=start;
 while(begin<end){int p=queue[begin++];for(int d:{-m.w,1,m.w,-1}){int q=p+d;if(q>=0&&q<m.w*m.h&&m.cells[q]&&parent[q]<0){parent[q]=p;queue[end++]=q;}}}
 assert(parent[goal]>=0);for(auto star:m.stars)assert(parent[star]>=0&&star!=start&&star!=goal);
 assert(m.stars[0]!=m.stars[1]&&m.stars[1]!=m.stars[2]&&m.stars[0]!=m.stars[2]);
 int n=0,p=goal;while(p!=start){path[n++]=p;p=parent[p];}assert(m.route_size==unsigned(n+1)&&m.route[0]==start&&m.route[n]==goal);
 for(auto star:m.stars)assert(std::find(path.begin(),path.begin()+n,star)!=path.begin()+n);
 if(seed<=30){for(int j=n-1;j>=0;--j){motion::Vec target{path[j]%m.w+.5f,path[j]/m.w+.5f};unsigned steps=0;
   for(;steps<1200;++steps){motion::Vec delta{target.x-m.ball.x,target.y-m.ball.y};if(delta.x*delta.x+delta.y*delta.y<.006f)break;
    m.tick(.016f,{std::clamp(delta.x*1.4f-m.velocity.x*.65f,-1.f,1.f),std::clamp(delta.y*1.4f-m.velocity.y*.65f,-1.f,1.f)});assert(!overlaps(m));if(m.won)break;}
   assert(steps<1200);marble::Profile saved;saved.unlocked=m.level;saved.selected=m.level;saved.snapshot(m);auto bytes=saved.encode();marble::Profile loaded;assert(loaded.decode(bytes));
   if(!m.won){Maze other=m;other.ball={1.5f,1.5f};assert(loaded.restore(other));assert(other.ball.x==m.ball.x&&other.ball.y==m.ball.y&&other.collected==m.collected);}
  }assert(m.won&&m.collected==7);marble::Profile progress;progress.unlocked=m.level;progress.finish(m);assert(progress.best[m.level]>0&&(progress.completed&(1u<<m.level)));assert(!progress.run);}
 m.generate(seed%12,wide,rng,tall);for(int i=0;i<2000;++i){m.tick(.05f,{float(int(rng.below(201))-100)/100,float(int(rng.below(201))-100)/100},1.4f);assert(!overlaps(m));}
 float time=m.elapsed;m.tick(0,{1,1});assert(time==m.elapsed);m.tick(1,{std::numeric_limits<float>::quiet_NaN(),0});assert(time==m.elapsed);
}
void dice_test(){Random rng;dice::Model d;assert(!d.valid&&d.selected==2);
 for(unsigned i=0;i<1000;++i){d.count(i%6+1);assert(!d.valid);assert(d.begin(rng));assert(!d.begin(rng));auto count=d.selected;d.count(6);assert(d.selected==count);assert(d.settle());assert(!d.settle());
  assert(d.history[0].count==count&&d.history[0].total()>=count&&d.history[0].total()<=count*6);assert(d.history_size==std::min(8u,i+1));
  auto bytes=d.encode();dice::Model other;assert(other.decode(bytes));assert(other.total_rolls==d.total_rolls&&other.result().total()==d.result().total());
  bytes[13]^=std::byte{1};assert(!other.decode(bytes));
 }
 auto sum=d.history[0].total();d.count(d.selected%6+1);assert(!d.valid&&d.history[0].total()==sum);auto bytes=d.encode();dice::Model other;assert(other.decode(bytes)&&!other.valid);
 bytes[4]=std::byte{7};motion::put32(bytes.data()+bytes.size()-4,motion::checksum(std::span(bytes).first(bytes.size()-4)));assert(!other.decode(bytes));
 std::array<unsigned,6> bins{};for(unsigned i=0;i<60000;++i)++bins[rng.below(6)];for(auto b:bins)assert(b>9000&&b<11000);
}
void motion_test(){motion::Motion m;m.calibrate();uint64_t now=1000000;for(unsigned i=0;i<30;++i){now+=50000;assert(!m.acceleration({0,0,9806650},now));}assert(m.calibrated&&!m.calibrating&&m.fresh(now));assert(!m.fresh(now+500000));
 for(unsigned i=0;i<30;++i){now+=50000;assert(!m.acceleration({0,-2451662,9498000},now));}assert(m.tilt().x>.4f);m.flip_x=true;assert(m.tilt().x<-.4f);m.flip_x=false;m.rotation=1;assert(m.tilt().y>.4f);
 m.suspend();now+=2000000;assert(!m.acceleration({0,0,9806650},now));assert(!m.acceleration({20000000,0,9806650},now+=150000));assert(m.acceleration({-20000000,0,9806650},now+=150000));assert(!m.acceleration({20000000,0,9806650},now+=150000));
 marble::Profile p;auto data=p.encode();marble::Profile q;assert(q.decode(data));data[24]^=std::byte{1};assert(!q.decode(data));
}
int main(){dice_test();motion_test();for(unsigned seed=1;seed<=500;++seed){maze_test(seed,false,false);maze_test(seed,true,false);maze_test(seed,false,true);}std::puts("PASS: dice count/history/codec/distribution; 1500 tiered mazes, 90 physics solves, 3M collision steps; exact resume, calibration, mirror, stale samples and shake debounce");}
