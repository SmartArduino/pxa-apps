// SPDX-License-Identifier: MIT
#include "../model.hpp"
#include <cassert>
#include <iostream>
using namespace iron;
void advance(World& w,float seconds){for(int i=0;i<int(seconds*20);++i)w.tick(.05f);}
int main(){
 {World w;w.init(0);assert(w.count(hq)==1&&w.count(hq,1)==1);int money=w.credits;advance(w,40);assert(w.earned>0&&w.credits>money);std::cout<<"Mining deposit: "<<w.earned<<'\n';}
 {World w;w.init(0);assert(!w.queue(tank));assert(w.queue(rifle));int credit=w.credits;w.cancel_job();assert(w.credits==credit+specs[rifle].cost);assert(w.queue(rocket));advance(w,5);assert(w.count(rocket)==2);}
 {World w;w.init(0);int money=w.credits;assert(!w.place(power,{5.5f,8.5f}));assert(w.credits==money);assert(w.place(factory,{9.f,5.f}));assert(w.count(factory)==1);advance(w,10);assert(w.count(factory)==1);assert(w.unlocked(tank));assert(w.place(power,{11.f,7.f}));advance(w,12);assert(w.queue(tank));advance(w,7);assert(w.count(tank)==3);}
 {World w;w.init(1);Vec from={10.5f,12.5f},to={18.5f,12.5f};for(int j=0;j<50;++j){Vec next=w.path(from,to,-1);assert(w.open(next));if(dist(from,to)<1)break;assert(dist(from,next)>0);from=next;}assert(dist(from,to)<1);std::cout<<"River path reaches objective\n";}
 {World w;w.init(0);w.select_all();assert(w.selected()==4);w.order({17,19});advance(w,26);assert(w.killed>0);assert(w.e[5].p.x>13);std::cout<<"Formation combat kills: "<<w.killed<<'\n';}
 {World w;w.init(0);int h=w.nearest({22,20},hq,1);w.damage(h,10000);advance(w,1);assert(w.won&&!w.defeated);}
 {World w;w.init(0);int h=w.nearest({5,8},hq,0);w.damage(h,10000);advance(w,1);assert(w.defeated);}
 {World w;w.init(1);w.e[5].p={20,12};for(auto& e:w.e)if(e.team==1&&e.kind<harvester)e.hp=0;advance(w,9);assert(w.count(relay,0)==1);for(auto& e:w.e)if(e.team==1)e.hp=0;w.radio_time=90;advance(w,1);assert(w.won);}
 {World w;w.init(2);for(auto& e:w.e)if(e.team==1&&e.kind==power)e.hp=0;advance(w,1);assert(w.story_beat);for(auto& e:w.e)if(e.team==1&&e.kind==turret)assert(e.cool>9000);}
 {World w;w.init(0);w.strike_cd=0;int credit=w.credits;assert(!w.strike({25,25}));assert(w.credits==credit);assert(w.strike({8,11}));assert(w.strike_cd==45);assert(!w.strike({8,11}));}
 std::cout<<"All RTS model checks passed\n";
}
