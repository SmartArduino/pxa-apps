// SPDX-License-Identifier: MIT
// A normal-economy strategy smoke test: build power, production and defenses,
// train a mixed army, then advance. No free credits, damage or time shortcuts.
#include "construction_strategy.hpp"
#include "../model.hpp"
#include <cassert>
#include <iostream>
using namespace iron;
int main(){for(int ch=0;ch<3;++ch){World w;w.init(ch);bool factory_ordered=ch>0,power_ordered=false,defense=false;int last_order=-1;
 for(int frame=0;frame<20*650&&!w.won&&!w.defeated;++frame){int t=int(w.time);
  if(!factory_ordered&&(w.credits>=500||w.build_active())){factory_ordered=build_at(w,factory,{5,21});}
  if(!power_ordered&&(w.credits>=200||w.build_active())){power_ordered=build_at(w,power,ch==0?Vec{7,20}:Vec{w.base_position().x-3,w.base_position().y-3});}
  if(!defense&&(w.credits>=250||w.build_active())&&t>20){defense=build_at(w,turret,campaign_maps[ch].front);}
  if(w.ready_count(factory)&&w.job_count<2&&w.army()<16){int k=w.count(tank)>3&&w.count(artillery)<3?artillery:tank;if(w.credits>=specs[k].cost)w.queue(k);}
  if(t!=last_order&&t%12==0&&t>70){last_order=t;w.select_all();Vec goal=w.objective_position();
   int target=w.hit(goal,1);
   if(ch==1&&w.count(relay,0)) goal={11,24};
   w.order(goal,target>=0&&w.e[target].team==1?target:-1);
   if(w.strike_cd<=0&&w.credits>=180){
    int enemy=w.nearest(goal,turret,1);
    if(enemy>=0&&w.fog[cell(w.e[enemy].p)])w.strike(w.e[enemy].p);
   }
  }
  w.tick(.05f);
 }
 std::cout<<"chapter="<<ch+1<<" time="<<w.time<<" won="<<w.won<<" lost="<<w.defeated<<" kills="<<w.killed<<" army="<<w.army()<<" mining="<<w.earned<<" relay="<<w.count(relay,0)<<" capture="<<w.capture<<" evac="<<w.radio_time<<" hq="<<w.count(hq,0)<<" credits="<<w.credits<<'\n'<<std::flush;
 if(ch!=1){if(!w.won){for(auto& a:w.e)if(a.hp>0&&a.team==1)std::cout<<" enemy="<<int(a.kind)<<" hp="<<a.hp<<" p="<<a.p.x<<","<<a.p.y<<"\n"<<std::flush;}assert(w.won);}else {if(!w.won){for(int i=0;i<MAX;++i){auto& a=w.e[i];if(a.hp>0)std::cout<<" e="<<i<<" kind="<<int(a.kind)<<" team="<<int(a.team)<<" hp="<<a.hp<<" p="<<a.p.x<<","<<a.p.y<<" goal="<<a.goal.x<<","<<a.goal.y<<" moving="<<a.moving<<"\n";}}assert(w.count(relay,0)&&w.won);}
}}
