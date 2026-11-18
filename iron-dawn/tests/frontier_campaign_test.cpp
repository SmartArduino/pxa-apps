// SPDX-License-Identifier: MIT
// Full large-map strategy smoke: normal economy, movement, combat and objectives.
#include "model.hpp"
#include <cassert>
#include <cstdio>
using namespace iron;
int main(){for(int ch=3;ch<6;++ch){World w;w.init(ch);int order=-1;bool power_ordered=false;
 for(int n=0;n<24000&&!w.won&&!w.defeated;++n){int t=int(w.time);if(!power_ordered&&w.credits>=200)power_ordered=w.place(power,{11,7});
  if(w.ready_count(factory)&&w.job_count<2&&w.army()<20){int kind=w.count(tank)>3&&w.count(artillery)<5?artillery:tank;if(w.credits>=specs[kind].cost)w.queue(kind);}
  if(t!=order&&t%6==0&&t>=36){order=t;w.select_all();int target=w.nearest({15,15},relay,2);Vec goal{30,28};if(target>=0)goal=w.e[target].p;else if(ch==3){target=w.nearest({25,22},factory,1);if(target>=0)goal=w.e[target].p;}else if(ch==4){for(int id:w.mission.convoy)if(w.e[id].hp>0){goal={w.e[id].p.x+1.5f,w.e[id].p.y+1.5f};break;}}else{target=w.nearest({32,30},w.mission.stage==1?power:hq,1);if(target>=0)goal=w.e[target].p;}w.order(goal,target>=0&&w.e[target].team==1?target:-1);
   if(w.strike_cd<=0&&w.credits>=180){int enemy=w.nearest(goal,turret,1);if(enemy<0&&target>=0&&w.e[target].team==1)enemy=target;if(enemy>=0&&w.fog[cell(w.e[enemy].p)])w.strike(w.e[enemy].p);}}
  w.tick(.05f);
 }
 std::printf("chapter=%d side=%d time=%.0f won=%d lost=%d army=%d losses=%d kills=%d nodes=%d delivered=%u credits=%d mined=%d\n",ch+1,w.side,w.time,w.won,w.defeated,w.army(),w.lost,w.killed,w.captured_nodes(),unsigned(w.mission.delivered),w.credits,w.earned);std::fflush(stdout);assert(w.won&&!w.defeated);
}}
