// SPDX-License-Identifier: MIT
#include "../geometry.hpp"
#include <cassert>
#include <cstdio>
#include <limits>
using namespace racer;
int main(){motion::Random random{0x5566};
 for(unsigned track=0;track<3;++track){Race race;race.begin(track);Profile p;p.track=track;float countdown=0;
  for(unsigned i=0;i<20000&&!race.finished;++i){float desired=0;for(unsigned pad=0;pad<6;++pad)if(race.pad_station(pad)>race.distance&&race.pad_station(pad)-race.distance<65){desired=race.pad_lane(pad);break;}
   for(auto car:race.traffic)if(car.distance-race.distance<24&&car.distance>race.distance&&std::abs(desired-car.lane)<.30f)desired=car.lane>0?-.6f:.6f;
   float feed=race.track.bend(race.distance)*.19f*(race.speed/70)*(race.speed/70)/(.50f+race.speed*.009f);race.tick(.033f,std::clamp((desired-race.x)*2+feed,-1.f,1.f));
   assert(std::isfinite(race.x)&&std::abs(race.x)<=1.48001f&&race.speed>=0&&race.speed<=120&&race.progress()<=1);p.snapshot(race,countdown);auto bytes=p.encode();Profile loaded;assert(loaded.decode(bytes));if(!race.finished){Race restored;restored.begin(track);assert(loaded.restore(restored,countdown));assert(restored.distance==race.distance&&restored.x==race.x&&restored.traffic[0].distance==race.traffic[0].distance);}
  }assert(race.finished&&race.elapsed>20&&race.elapsed<180&&race.rank()>=1&&race.rank()<=4);p.finish(race);assert(!p.run&&p.best[track]&&p.best_rank[track]);auto bytes=p.encode();Profile q;assert(q.decode(bytes));bytes[40]^=std::byte{1};assert(!q.decode(bytes));
  Race off;off.begin(track);for(int i=0;i<3000;++i)off.tick(.05f,1);assert(std::abs(off.x)<=1.48001f&&off.speed<=27);
  Race collision;collision.begin(track);collision.distance=40;collision.x=collision.traffic[0].lane;collision.speed=70;collision.tick(.016f,0);assert(collision.collisions==1&&collision.speed<50);collision.tick(.016f,0);assert(collision.collisions==1);
  Race turbo;turbo.begin(track);turbo.distance=turbo.pad_station(0)-.1f;turbo.x=turbo.pad_lane(0);turbo.speed=60;turbo.tick(.016f,0);assert(turbo.pads==1&&turbo.boost>3);
  float previous=turbo.elapsed;turbo.tick(std::numeric_limits<float>::quiet_NaN(),0);turbo.tick(.033f,std::numeric_limits<float>::infinity());assert(turbo.elapsed==previous);
  Camera camera;for(unsigned trial=0;trial<10000;++trial){race.distance=float(random.below(100000))/10;race.x=float(int(random.below(297))-148)/100;camera.setup(race,trial&1?160:400,40,trial&1?130:350);
   auto frame=frame_at(race.track,race.distance);assert(std::abs(frame.nx*frame.nx+frame.nz*frame.nz-1)<.0001f);
   std::array<V3,4> poly{};for(auto& v:poly)v={float(int(random.below(300))-150),float(int(random.below(100))-50),float(int(random.below(500))-5)};
   // Clip a well-ordered quad rather than an arbitrary self-crossing polygon.
   float z=float(random.below(500))*.1f-.5f;poly={V3{-10,-5,z},V3{10,-5,z+3},V3{10,10,z+3},V3{-10,10,z}};std::array<V3,12> near{};auto n=clip_near(poly,near);std::array<P2,12> points{},out{};
   for(unsigned i=0;i<n;++i){assert(near[i].z>=.6999f);points[i]=camera.project(near[i]);assert(std::isfinite(points[i].x)&&std::isfinite(points[i].y));}n=clip_screen(std::span(points).first(n),{0,0,400,400},out);assert(n<=9);
   for(unsigned i=0;i<n;++i)assert(out[i].x>=-.01f&&out[i].x<=400.01f&&out[i].y>=-.01f&&out[i].y<=400.01f);
  }
 }
 std::puts("PASS: all three complete two-lap races, exact checkpoints, collisions, boosts, off-road limits, malformed state and 30000 clipped 3D projections");}
