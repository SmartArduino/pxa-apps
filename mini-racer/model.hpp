// SPDX-License-Identifier: MIT
#pragma once
#include "../common/motion-games/motion.hpp"
#include "../common/motion-games/codec.hpp"
#include <bit>
#include <limits>
namespace racer {
constexpr float pi=3.14159265358979323846f;
struct Track {
 static constexpr unsigned samples=256;
 struct Point {float x=0,y=0;};
 std::array<Point,samples+1> points{};unsigned id=0;float length=1800;
 void generate(unsigned which){id=std::min(2u,which);length=1800+float(id)*400;
   for(unsigned i=0;i<=samples;++i){float a=2*pi*i/samples;
     points[i]={std::sin(a)*(id==0?110.f:150.f)+std::sin(a*3)*(id==0?23.f:40.f),
       std::sin(a*2)*(id==0?4.f:id==1?15.f:7.f)+std::sin(a*3)*2};}
   points[samples]=points[0];
 }
 Point at(float s)const{float u=std::fmod(s,length);if(u<0)u+=length;float f=u*samples/length;unsigned i=std::min(samples-1,unsigned(f));f-=i;
   return {points[i].x+(points[i+1].x-points[i].x)*f,points[i].y+(points[i+1].y-points[i].y)*f};}
 float tangent(float s)const{return (at(s+5).x-at(s-5).x)*.1f;}
 float bend(float s)const{return std::clamp((tangent(s+30)-tangent(s-30))*12.f,-1.3f,1.3f);}
};
struct Traffic {float distance=0,lane=0,speed=0;unsigned color=0;};
struct Race {
 Track track;std::array<Traffic,3> traffic{};
 float distance=0,x=0,speed=0,elapsed=0,steering=0,boost=0,collision_cooldown=0;
 unsigned pads=0,collisions=0,passes=0;bool finished=false;
 void begin(unsigned id){track.generate(id);distance=x=speed=elapsed=steering=boost=collision_cooldown=0;pads=collisions=passes=0;finished=false;
   for(unsigned i=0;i<traffic.size();++i)traffic[i]={40.f+i*38.f,(float(i)-1)*.60f+std::sin(i*1.8f)*.09f,43.f+i*4.f+id*2.f,i};}
 float progress()const{return std::clamp(distance/(track.length*2),0.f,1.f);}
 unsigned lap()const{return std::min(2u,1+unsigned(std::max(0.f,distance)/track.length));}
 unsigned rank()const{unsigned n=1;for(auto car:traffic)if(car.distance>distance)++n;return n;}
 float pad_station(unsigned index)const{return (float(index/3)+(.20f+.29f*float(index%3)))*track.length;}
 float pad_lane(unsigned index)const{return float(int(index%3)-1)*.60f;}
 void tick(float dt,float input,float sensitivity=1){
   if(finished||!(dt>0)||!std::isfinite(dt)||!std::isfinite(input)||!std::isfinite(sensitivity))return;
   dt=std::min(dt,.05f);input=std::clamp(input*sensitivity,-1.f,1.f);elapsed+=dt;
   steering+=(input-steering)*std::min(1.f,dt*8);boost=std::max(0.f,boost-dt);collision_cooldown=std::max(0.f,collision_cooldown-dt);
   float max_speed=(70+track.id*4)+(boost>0?23:0);if(std::abs(x)>1.03f)max_speed=26;
   float accel=speed<max_speed?11.f:-34.f;speed=std::clamp(speed+accel*dt,0.f,max_speed>speed?max_speed:120.f);
   if(accel<0)speed=std::max(speed,max_speed);
   x+=steering*(.50f+speed*.009f)*dt-track.bend(distance)*.19f*(speed/70)*(speed/70)*dt;x=std::clamp(x,-1.48f,1.48f);
   float old=distance;distance=std::min(track.length*2,distance+speed*dt);
   for(unsigned i=0;i<6;++i){float station=pad_station(i);if(!(pads&(1u<<i))&&old<station&&distance>=station&&std::abs(x-pad_lane(i))<.24f){pads|=1u<<i;boost=3.4f;}}
   for(unsigned i=0;i<traffic.size();++i){auto &car=traffic[i];float before=car.distance;car.distance+=car.speed*dt;car.lane=std::clamp((float(i)-1)*.60f+std::sin(elapsed*.22f+i*1.8f)*.09f,-.83f,.83f);
     if(old<=before&&distance>car.distance)++passes;
     if(!collision_cooldown&&std::abs(distance-car.distance)<2.8f&&std::abs(x-car.lane)<.23f){speed=std::max(18.f,speed*.55f);collision_cooldown=1.25f;++collisions;}}
   if(distance>=track.length*2){finished=true;boost=0;}
 }
};
struct Profile {
 static constexpr unsigned size=128;
 unsigned track=0,control=0,sensitivity=1,color=0,rotation=0;bool mirror=false;unsigned completed=0,races=0;
 std::array<uint32_t,3> best{};std::array<uint8_t,3> best_rank{};
 bool run=false;unsigned run_track=0;std::array<float,8> state{};std::array<float,3> car_distances{};unsigned saved_pads=0,saved_hits=0,saved_passes=0;
 void snapshot(const Race& race,float countdown){run=!race.finished;run_track=race.track.id;state={race.x,race.distance,race.speed,race.elapsed,race.steering,race.boost,race.collision_cooldown,countdown};saved_pads=race.pads;saved_hits=race.collisions;saved_passes=race.passes;for(unsigned i=0;i<3;++i)car_distances[i]=race.traffic[i].distance;}
 bool restore(Race& race,float& countdown)const{if(!run||run_track!=race.track.id)return false;race.x=state[0];race.distance=state[1];race.speed=state[2];race.elapsed=state[3];race.steering=state[4];race.boost=state[5];race.collision_cooldown=state[6];countdown=state[7];race.pads=saved_pads;race.collisions=saved_hits;race.passes=saved_passes;
   for(unsigned i=0;i<3;++i){race.traffic[i].distance=car_distances[i];race.traffic[i].lane=std::clamp((float(i)-1)*.60f+std::sin(race.elapsed*.22f+i*1.8f)*.09f,-.83f,.83f);}return true;}

 void finish(const Race& race){if(!race.finished)return;unsigned i=race.track.id;run=false;completed|=1u<<i;if(races!=UINT32_MAX)++races;
   auto ms=uint32_t(std::clamp(race.elapsed*1000.f,1.f,86400000.f));if(!best[i]||ms<best[i])best[i]=ms;auto rank=race.rank();if(!best_rank[i]||rank<best_rank[i])best_rank[i]=uint8_t(rank);}
 std::array<std::byte,size> encode()const{std::array<std::byte,size> data{};motion::put32(data.data(),0x52414331);data[4]=std::byte(track);data[5]=std::byte(control);data[6]=std::byte(sensitivity);data[7]=std::byte(color);data[8]=std::byte(rotation);data[9]=std::byte(mirror);data[10]=std::byte(completed);motion::put32(data.data()+12,races);
   for(unsigned i=0;i<3;++i){motion::put32(data.data()+16+4*i,best[i]);data[28+i]=std::byte(best_rank[i]);}data[31]=std::byte(run);data[32]=std::byte(run_track);for(unsigned i=0;i<8;++i)motion::put32(data.data()+36+4*i,std::bit_cast<uint32_t>(state[i]));motion::put32(data.data()+68,saved_pads);motion::put32(data.data()+72,saved_hits);motion::put32(data.data()+76,saved_passes);for(unsigned i=0;i<3;++i)motion::put32(data.data()+80+4*i,std::bit_cast<uint32_t>(car_distances[i]));motion::put32(data.data()+124,motion::checksum(std::span(data).first(124)));return data;}
 bool decode(std::span<const std::byte> data){if(data.size()!=size||motion::get32(data.data())!=0x52414331||motion::get32(data.data()+124)!=motion::checksum(data.first(124)))return false;
   Profile p;p.track=unsigned(data[4]);p.control=unsigned(data[5]);p.sensitivity=unsigned(data[6]);p.color=unsigned(data[7]);p.rotation=unsigned(data[8]);p.mirror=bool(data[9]);p.completed=unsigned(data[10]);p.races=motion::get32(data.data()+12);
   if(p.track>2||p.control>2||p.sensitivity>2||p.color>2||p.rotation>3||unsigned(data[9])>1||p.completed>7||data[11]!=std::byte{})return false;
   for(unsigned i=0;i<3;++i){p.best[i]=motion::get32(data.data()+16+4*i);p.best_rank[i]=uint8_t(data[28+i]);bool done=p.completed&(1u<<i);
     if(p.best[i]>86400000||p.best_rank[i]>4||(done?(!p.best[i]||!p.best_rank[i]):(p.best[i]||p.best_rank[i])))return false;}
   if(unsigned(data[31])>1||unsigned(data[32])>2)return false;p.run=bool(data[31]);p.run_track=unsigned(data[32]);for(unsigned i=0;i<8;++i){p.state[i]=std::bit_cast<float>(motion::get32(data.data()+36+4*i));if(!std::isfinite(p.state[i]))return false;}
   p.saved_pads=motion::get32(data.data()+68);p.saved_hits=motion::get32(data.data()+72);p.saved_passes=motion::get32(data.data()+76);
   for(unsigned i=0;i<3;++i){p.car_distances[i]=std::bit_cast<float>(motion::get32(data.data()+80+4*i));if(!std::isfinite(p.car_distances[i])||p.car_distances[i]<0||p.car_distances[i]>10000000)return false;}
   if(std::abs(p.state[0])>1.48001f||p.state[1]<0||p.state[1]>2*(1800+400*p.run_track)||p.state[2]<0||p.state[2]>120||p.state[3]<0||p.state[3]>86400||std::abs(p.state[4])>1.001f||p.state[5]<0||p.state[5]>3.40001f||p.state[6]<0||p.state[6]>1.25001f||p.state[7]<0||p.state[7]>3||p.saved_pads>63||p.saved_hits>1000000||p.saved_passes>1000000)return false;
   for(unsigned i=33;i<36;++i)if(data[i]!=std::byte{})return false;for(unsigned i=92;i<124;++i)if(data[i]!=std::byte{})return false;*this=p;return true;}
};
}
