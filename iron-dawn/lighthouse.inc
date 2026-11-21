// SPDX-License-Identifier: MIT
 constexpr static Vec ferry_start(unsigned i){return {61.5f,38.5f+i*7.f};}

 void init_lighthouse(){
  terrain.fill(water);
  patch(ground,1,1,24,side-2);
  patch(ground,38,7,21,42);
  landform(ground,48,9,10,6);landform(ground,50,46,9,7);
  landform(water,38,9,5,6);landform(water,58,46,4,6);
  landform(woodland,12,25,7,10);landform(woodland,40,40,3,6);
  landform(ridge,46,22,4,8);landform(ridge,14,14,5,5);
  landform(crater,51,31,6,5);landform(woodland,46,11,4,3);
  route({{16,44},{22,44},{22,45},{53,45}},true);
  route({{43,45},{43,33},{53,33},{53,14},{55,14},{55,11}});
  route({{19,44},{19,31},{23,31}});
  route({{39,31},{43,31},{43,33}});
  deposit(7,43);deposit(17,54);deposit(47,43);

  deploy(0, {{hq,{16,43}}, {power,{12,40}}, {refinery,{10,46}}, {barracks,{16,37}},
             {harvester,{8.5f,44.5f}}, {factory,{19,37}}, {tank,{19,43}}, {tank,{20,45}},
             {rifle,{18,42}}, {rocket,{18,44}}, {artillery,{15,48}}, {commander,{15,45}},
             {power,{14,49}}, {power,{8,36}}, {shipyard,{24,38}}, {landing_craft,{25.5f,37.5f}},
             {patrol_boat,{27.5f,39.5f}}, {helipad,{18,51}}, {rotorcraft,{18,51}}});
  deploy(1, {{hq,{55,11}}, {factory,{51,12}}, {power,{56,27}}, {tank,{49,33.5f}},
             {rocket,{48,29}}, {tank,{50,17}}, {aa_tower,{56,19}}, {turret,{45,34}},
             {helipad,{51,24}}, {rotorcraft,{51,24}}, {patrol_boat,{31.5f,22.5f}}});
  deploy(2, {{relay,island_grid_site}, {relay,lighthouse_site}, {supply,{22,33}}, {supply,{47,42}}});
  for(unsigned i=0;i<2;++i){
   int id=add(landing_craft,2,ferry_start(i));
   mission.convoy[i]=int16_t(id);
   if(id>=0)e[id].dir=-1.5707963f;
  }
  if(mission.harbor_plan==harbor_beacon){
   deploy(0, {{rifle,{39.5f,30.5f}}, {rocket,{39.5f,32.5f}}, {patrol_boat,{36.5f,30.5f}}});
  }else if(mission.harbor_plan==harbor_shutdown){
   route({{23,31},{39,31}},true);
  }
 }

 int lighthouse_landmark(const Entity& a)const{
  if(extra.mode||chapter!=lighthouse_chapter||a.kind!=relay)return -1;
  if(distance_squared(a.p,island_grid_site)<.01f)return 0;
  return distance_squared(a.p,lighthouse_site)<.01f?1:-1;
 }

 int mission_landmark(const Entity& a)const{
  int harbor=harbor_landmark(a);
  if(harbor>=0)return harbor;
  int island=lighthouse_landmark(a);
  return island<0?-1:island+2;
 }

 const char* lighthouse_pause_reason()const{
  if(power_net<0)return "缺电";
  auto present=node_presence(lighthouse_site);
  if(present.enemy)return "有敌军";
  if(!present.friendly)return "缺守军";
  return nullptr;
 }

 void lighthouse_tick(float dt){
  const bool need_grid=mission.stage==restore_grid,need_light=mission.stage!=guide_ferries;
  int grid=relay_at(island_grid_site,need_grid?2:0);
  int light=relay_at(lighthouse_site,need_light?2:0);
  if(grid<0||light<0){defeated=true;return;}
  if(need_grid){
   if(capture_node(e[grid],dt)){
    mission.stage=capture_lighthouse;
    mission.events|=1u<<island_grid_radio;
   }
   return;
  }
  if(need_light){
   if(capture_node(e[light],dt)){
    mission.stage=guide_ferries;
    mission.events|=1u<<lighthouse_radio;
    next_wave=std::min(next_wave,time+8.f);
   }
   return;
  }
  if(lighthouse_pause_reason())return;
  radio_time=std::min(lighthouse_hold_seconds,radio_time+dt);
  float fraction=radio_time/lighthouse_hold_seconds;
  for(unsigned i=0;i<2;++i){
   int id=mission.convoy[i];
   if(id<0||id>=MAX)continue;
   auto& ferry=e[id];
   if(!deployed(ferry)||ferry.team!=2||ferry.kind!=landing_craft)continue;
   auto p=ferry_start(i);
   p.y-=32.f*fraction;
   ferry.p=ferry.goal=ferry.step=p;
  }
  won=radio_time>=lighthouse_hold_seconds;
 }
