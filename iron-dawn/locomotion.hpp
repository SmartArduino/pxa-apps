// SPDX-License-Identifier: MIT
#pragma once
#include "model.hpp"
namespace iron {
// Transient visual state. Orders are intent; only actual movement advances feet.
struct Locomotion {
 struct Pose {Vec last{};float phase=0,weight=0;uint8_t kind=255,team=255,token=0;bool alive=false;};
 std::array<Pose,MAX> poses{};float last_time=-1;
 void update(const World& w){
  if(w.time==last_time)return;
  float dt=w.time-last_time;bool reset=last_time<0||dt<=0||dt>.5f;last_time=w.time;
  for(int i=0;i<MAX;++i){auto& p=poses[i];const auto& a=w.e[i];bool alive=deployed(a)&&unit_kind(a.kind);uint8_t token=combat_kind(a.kind)?a.cargo:0;
   if(!alive){p.alive=false;p.weight=0;continue;}
   if(reset||!p.alive||p.kind!=a.kind||p.team!=a.team||p.token!=token){p={a.p,0,0,a.kind,a.team,token,true};continue;}
   float moved=dist(a.p,p.last);p.last=a.p;
   if(moved>1){p.phase=p.weight=0;continue;}
   bool walking=moved>.00005f;
   if(walking)p.phase=std::fmod(p.phase+moved/(a.kind<=rocket?.72f:.64f),1.f);
   p.weight=walking?std::min(1.f,p.weight+dt*12):std::max(0.f,p.weight-dt*12);
  }
 }
 const Pose& get(int id)const{return poses[std::clamp(id,0,MAX-1)];}
};
}
