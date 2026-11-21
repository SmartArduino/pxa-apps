// SPDX-License-Identifier: MIT
// The first station secured decides which evacuation plan the crew follows.
 int harbor_landmark(const Entity& a)const{
  if(chapter!=harbor_chapter||extra.mode||a.kind!=relay)return -1;
  return distance_squared(a.p,harbor_beacon_site)<.01f?0:1;
 }

 bool enemy_defences_powered()const{
  if(chapter==harbor_chapter&&mission.stage==harbor_shutdown)return false;
  return (chapter!=2&&chapter!=5&&chapter!=harbor_chapter)||count(power,1)>0;
 }

 bool harbor_signal_active()const{
  auto present=node_presence(harbor_beacon_site);
  return power_net>=0&&present.friendly&&!present.enemy;
 }

 void harbor_tick(float dt){
  if(mission.stage==harbor_choice){
   int beacon=relay_at(harbor_beacon_site,2),station=relay_at(harbor_switch_site,2);
   if(beacon<0&&station<0){defeated=true;return;}
   if(beacon>=0&&capture_node(e[beacon],dt)){
    mission.stage=harbor_beacon;
    mission.events|=1u<<harbor_beacon_radio;
    next_wave=time+9;
   }else if(station>=0&&capture_node(e[station],dt)){
    mission.stage=harbor_shutdown;
    mission.events|=1u<<harbor_shutdown_radio;
   }
   return;
  }
  bool signaling=mission.stage==harbor_beacon;
  Vec site=signaling?harbor_beacon_site:harbor_switch_site;
  if(relay_at(site,0)<0){defeated=true;return;}
  if(!signaling){won=!count(hq,1);return;}
  if(harbor_signal_active())
   radio_time=std::min(harbor_hold_seconds,radio_time+dt);
  won=radio_time>=harbor_hold_seconds;
 }

 void harbor_raid(){
  ++wave;next_wave=time+19;
  bool north=wave%2;
  auto send=[&](int kind,Vec entry,Vec goal){
   int id=add(kind,1,entry);
   if(id>=0){e[id].goal=goal;e[id].moving=true;}
  };
  send(patrol_boat,wave_entry(wave),
       north?Vec{30.5f,18.5f}:Vec{30.5f,32.5f});
  if(wave>1)send(rotorcraft,north?Vec{23.5f,10.5f}:Vec{23.5f,43.5f},harbor_beacon_site);
  say("苏弥：巡逻艇正沿航道靠近，别让它们打断信标。",6);
 }
