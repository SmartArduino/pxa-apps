// SPDX-License-Identifier: MIT
#include "unit_voice.hpp"
#include "save.hpp"
#include <cassert>
#include <cstdio>
using namespace iron;
int main(){
 World w;int enemy=w.add(tank,1,{8,8}),building=w.add(hq,0,{5,5}),miner=w.add(harvester,0,{6,6});w.e[enemy].selected=w.e[building].selected=w.e[miner].selected=true;
 assert(UnitVoice::speaker(w)==harvester);assert(UnitVoice::speaker(w,enemy,-1,true)==-1);
 int a=w.add(rifle,0,{7,7}),b=w.add(rocket,0,{7,8});w.e[a].selected=w.e[b].selected=true;
 assert(UnitVoice::speaker(w,b)==rocket);assert(UnitVoice::speaker(w,-1,rifle)==rifle);assert(UnitVoice::speaker(w,building,-1,true)==rifle);
 auto before=UnitVoice::roster(w);assert(!before[enemy]&&!before[building]&&before[miner]&&before[a]);int fresh=w.add(artillery,0,{9,9});assert(!before[fresh]&&UnitVoice::roster(w)[fresh]);
 for(int kind=0;kind<5;++kind){UnitVoice v;assert(v.offer(kind,UnitCue::select,1000));int first=v.take(1000);assert(first==kind*8&&voice_files[first]);v.started(first,1000);assert(!v.offer(kind,UnitCue::select,1100));assert(v.take(1100)==-1);assert(v.offer(kind,UnitCue::select,6000));assert(v.take(6000)==kind*8+1);}
 UnitVoice v;assert(v.offer(tank,UnitCue::select,1000));int current=v.take(1000);v.started(current,1000);
 assert(v.offer(tank,UnitCue::move,2000));assert(v.offer(tank,UnitCue::attack,2100));assert(!v.offer(rifle,UnitCue::ready,2150));assert(v.take(2200)==-1);assert(v.take(v.busy_until)==tank*8+4);
 assert(v.offer(rocket,UnitCue::move,6000));assert(v.take(7300)==-1); // expired orders never play later
 assert(v.offer(rifle,UnitCue::ready,8000));unsigned serial=v.sequence;v.clear();assert(v.sequence!=serial&&v.take(9000)==-1&&v.busy_until==0);
 assert(!v.offer(harvester,UnitCue::attack,10000)&&!v.offer(harvester,UnitCue::stop,10000));assert(!v.offer(hq,UnitCue::select,10000));
 assert(v.offer(artillery,UnitCue::stop,11000)&&v.take(11000)==artillery*8+6);assert(v.offer(harvester,UnitCue::ready,12000)&&v.take(12000)==harvester*8+7);
 unsigned clips=0;for(int i=0;i<40;++i)if(voice_files[i]){++clips;assert(voice_lengths[i]>300&&voice_lengths[i]<4500);}assert(clips==37&&sizeof(Save)<31000);
 std::puts("Five fixed voice casts, all 37 cue mappings, ally selection, spawn observation, radio arbitration, expiration, cancellation and unchanged V8 save roundtrip passed");
}
