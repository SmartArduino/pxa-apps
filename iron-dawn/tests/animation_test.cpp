// SPDX-License-Identifier: MIT
#include "art.hpp"
#include "save.hpp"
#include <cassert>
#include <cstdio>
#include <vector>
using namespace iron;
static bool close(float a,float b,float eps=.001f){return std::abs(a-b)<eps;}
int main(){
 {World w;w.init(0);Locomotion motion;motion.update(w);auto& a=w.e[7];a.moving=true;
  for(int i=0;i<30;++i){w.time+=1/30.f;motion.update(w);}assert(motion.get(7).weight==0&&motion.get(7).phase==0);
  for(int i=0;i<30;++i){a.p.x+=.01f;w.time+=1/30.f;motion.update(w);}assert(motion.get(7).weight>.99f&&close(motion.get(7).phase,.3f/.72f));
  float phase=motion.get(7).phase;for(int i=0;i<30;++i)motion.update(w);assert(motion.get(7).phase==phase);
  for(int i=0;i<10;++i){w.time+=1/30.f;motion.update(w);}assert(a.moving&&motion.get(7).weight==0&&motion.get(7).phase==phase);
  ++a.cargo;w.time+=.033f;motion.update(w);assert(motion.get(7).phase==0&&motion.get(7).weight==0);w.time=0;motion.update(w);assert(motion.get(7).weight==0);
 }
 auto south=CombatArt::socket(rocket,1,0,0,1);assert(south.x<-3&&south.y<-15); // Front-facing launcher bore is left of the chest.
 assert(art::muzzles[rifle][5].behind&&art::muzzles[rocket][5].behind);
 World world;
 unsigned poses=0,sockets=0;
 for(int kind=0;kind<2;++kind)for(unsigned d=0;d<8;++d){
  float angle=d*3.14159265f/4;Pt direction{20*(std::cos(angle)-std::sin(angle)),10*(std::cos(angle)+std::sin(angle))};
  for(int frame=0;frame<3;++frame){auto a=CombatArt::gait(kind,d,frame/8.f,1),b=CombatArt::gait(kind,d,(frame+1)/8.f,1);assert(close(b.legs[0].ankle.x-a.legs[0].ankle.x+direction.x*.72f/8,0));assert(close(b.legs[0].ankle.y-a.legs[0].ankle.y+direction.y*.72f/8,0));}
  const auto& f=art::frames[art::unit_first[kind]+d];auto rig=art::infantry_rigs[kind][d];std::vector<uint16_t> stable;
  for(int frame=0;frame<8;++frame){auto g=CombatArt::gait(kind,d,frame/8.f,1);assert(g.legs[0].travel*g.legs[1].travel<=.001f);assert(g.legs[0].lift*g.legs[1].lift<.0001f);
   if(frame==0||frame==4)assert(g.legs[0].lift<.001f&&g.legs[1].lift<.001f);
   if(frame==2)assert(g.legs[0].lift==0&&g.legs[1].lift>2.1f);
   if(frame==6)assert(g.legs[1].lift==0&&g.legs[0].lift>2.1f);
   for(auto leg:g.legs){assert(std::hypot(leg.hip.x-leg.knee.x,leg.hip.y-leg.knee.y)<12);assert(std::hypot(leg.ankle.x-leg.knee.x,leg.ankle.y-leg.knee.y)<12);}
   constexpr int w=80,h=80;std::vector<uint16_t> pixels(w*h+2,navy);pixels.front()=pixels.back()=0xa55a;Raster r(pixels.data()+1,w,h,w);CombatArt::infantry(r,kind,d,40,60,1,frame/8.f,1,0);assert(pixels.front()==0xa55a&&pixels.back()==0xa55a);
   int cut=60-f.oy+rig.hip_y-3;std::vector<uint16_t> upper(pixels.begin()+1,pixels.begin()+1+cut*w);if(frame==0)stable=upper;else assert(stable==upper);++poses;
  }
 }
 for(unsigned kind=0;kind<4;++kind)for(unsigned d=0;d<8;++d){const auto& f=art::frames[art::unit_first[kind]+d];auto socket=art::muzzles[kind][d];bool adjacent=false;auto bytes=art::bytes.data()+f.offset;
  for(unsigned i=0;i<f.count;++i){int x=bytes[0],y=bytes[1],n=bytes[2];bytes+=3;if(std::abs(y-socket.y)<=3&&socket.x>=x-3&&socket.x<=x+n+3)adjacent=true;bytes+=n;}assert(adjacent);
  for(float z:{.45f,.72f,1.f,1.3f}){Battlefield field;field.width=480;field.top=50;field.bottom=314;field.zoom=z;field.focus({12,12});Shot shot;shot.kind=kind;shot.from={12,12};float angle=d*3.14159265f/4;shot.dest={12+4*std::cos(angle),12+4*std::sin(angle)};shot.duration=.85f;shot.alive=true;
   auto origin=field.project(shot.from);auto expected=CombatArt::socket(kind,d,std::lround(origin.x),std::lround(origin.y),z);auto start=field.muzzle(shot);assert(start.x==expected.x&&start.y==expected.y);auto end=field.project(shot.dest,16);auto p0=CombatArt::trajectory(start,end,0,kind==artillery,z,d),p1=CombatArt::trajectory(start,end,1,kind==artillery,z,d);assert(p0.x==start.x&&p0.y==start.y&&p1.x==end.x&&p1.y==end.y);
   if(kind==artillery){auto next=CombatArt::trajectory(start,end,.0001f,true,z,d),axis=CombatArt::shell_axis(d);float dx=next.x-start.x,dy=next.y-start.y;assert((dx*axis.x+dy*axis.y)/std::hypot(dx,dy)/std::hypot(axis.x,axis.y)>.99f);}
   std::vector<uint16_t> pixels(480*360+2,navy);pixels.front()=pixels.back()=0xa55a;Raster r(pixels.data()+1,480,360,480);field.draw_shot(r,shot,world);assert(pixels.front()==0xa55a&&pixels.back()==0xa55a);assert(r.pixels[int(start.y)*480+int(start.x)]==cream);++sockets;
  }
 }
 for(unsigned d=0;d<8;++d)for(float z:{.45f,.72f,1.f,1.3f}){Battlefield field;field.zoom=z;field.focus({12,12});Shot shot;shot.kind=turret;shot.from={12,12};float angle=d*3.14159265f/4;shot.dest={12+4*std::cos(angle),12+4*std::sin(angle)};auto a=field.muzzle(shot),origin=field.project(shot.from);auto& f=art::frames[art::turret_heads_first+8+d];auto tip=art::turret_muzzles[8+d];Pt b{origin.x+(tip.x-f.ox)*z,origin.y+(tip.y-f.oy-art::turret_mount_height)*z};assert(close(a.x,b.x)&&close(a.y,b.y));auto steady=TurretArt::muzzle(d,false),recoil=TurretArt::muzzle(d,true);Pt axis{20*(std::cos(angle)-std::sin(angle)),10*(std::cos(angle)+std::sin(angle))};assert((steady.x-recoil.x)*axis.x+(steady.y-recoil.y)*axis.y>0);++sockets;}
 for(int kind:{int(rifle),int(rocket)}){World w;w.init(0);for(auto& a:w.e)a.hp=0;int id=w.add(kind,0,{12,12});w.e[id].dir=5*3.14159265f/4;w.fog.fill(1);w.seen.fill(1);Battlefield field;field.width=480;field.top=50;field.bottom=314;field.focus({12,12});std::vector<uint16_t> a(480*480,navy),b=a;Raster ra(a.data(),480,480,480),rb(b.data(),480,480,480);field.draw(ra,w);auto& shot=w.shots[0];shot.kind=kind;shot.from={12,12};shot.dest={9,9};shot.duration=1;shot.alive=true;auto socket=field.muzzle(shot);field.draw(rb,w);assert(a[int(socket.y)*480+int(socket.x)]==b[int(socket.y)*480+int(socket.x)]);}
 {World w;w.init(0);auto& a=w.e[5];int enemy=w.add(rifle,1,{10,12});a.flash=.04f;w.fire(5,enemy);assert(a.flash==.04f);for(auto b:w.blasts)assert(!b.alive||b.type!=5);for(auto s:w.shots)if(s.alive){assert(s.from.x==a.p.x&&s.from.y==a.p.y&&shot_source(s)==5);}}
 std::printf("%u infantry frames: opposite legs, planted stance, fixed torso; %u muzzle/trajectory/zoom cases; movement/intent/pause/reuse/save layout passed\n",poses,sockets);
}
