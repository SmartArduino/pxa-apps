// SPDX-License-Identifier: MIT
// Reinforcements rally in wings, then work toward shared mission objectives.
 static constexpr float raid_rally_limit=6.f;

 bool dual_raid_entry(int next)const{
  if(extra.mode)return next>=3&&next%3==0;
  if(chapter==harbor_chapter&&mission.stage==harbor_beacon)return false;
  return authored_map()&&(chapter==4||next>=2);
 }

 float raid_interval()const{
  if(extra.mode)return raid_rally_limit;
  return authored_map()?std::max(42.f,68.f-wave*3.f):std::max(24.f,55.f-wave*2.f-chapter*4.f);
 }

 int pursued_convoy()const{
  if(extra.mode||chapter!=4)return -1;
  int lead=-1;
  float closest=0;
  for(int j=0;j<int(mission.convoy.size());++j){
   int id=mission.convoy[j];
   if((mission.delivered&(1u<<j))||!active_convoy(id))continue;
   float remaining=distance_squared(e[id].p,convoy_stop(3));
   if(lead<0||remaining<closest){lead=id;closest=remaining;}
  }
  return lead;
 }

 Vec raid_objective(RaidRole role,Vec base)const{
  if(role==RaidRole::assault)return base;
  if(role==RaidRole::intercept&&!extra.mode){
   if(chapter==lighthouse_chapter)return objective_position();
   int id=pursued_convoy();
   if(id>=0)return e[id].p;
   id=nearest(wave_entry(wave),relay,0);
   if(id>=0)return e[id].p;
  }
  int target=nearest(base,refinery,0);
  if(target<0)target=nearest(base,power,0);
  return target<0?base:e[target].p;
 }

 Vec raid_open_point(int kind,Vec desired,Vec fallback)const{
  desired.x=std::clamp(desired.x,1.5f,side-1.5f);
  desired.y=std::clamp(desired.y,1.5f,side-1.5f);
  if(open_for(kind,desired))return desired;
  float best=100;
  for(int y=std::max(1,int(desired.y)-2);y<=std::min(side-2,int(desired.y)+2);++y)
   for(int x=std::max(1,int(desired.x)-2);x<=std::min(side-2,int(desired.x)+2);++x){
    Vec p=center(y*N+x);
    float d=distance_squared(desired,p);
    if(d<best&&open_for(kind,p)){best=d;fallback=p;}
   }
  return fallback;
 }

 struct RaidIntel {bool armor=false,air=false,fortified=false;};
 RaidIntel scout_raid()const{
  RaidIntel observed;
  for(const auto& asset:e){
   if(!deployed(asset)||asset.team!=0)continue;
   bool armor=!observed.armor&&(asset.kind==tank||asset.kind==artillery);
   bool air=!observed.air&&asset.kind==rotorcraft;
   bool fortified=!observed.fortified&&defence_kind(asset.kind)&&asset.progress<=0;
   if(!armor&&!air&&!fortified)continue;
   for(const auto& scout:e){
    if(!deployed(scout)||scout.team!=1)continue;
    float sight=scout.kind==rotorcraft?9.f:6.f;
    if(distance_squared(asset.p,scout.p)>sight*sight)continue;
    observed.armor|=armor;observed.air|=air;observed.fortified|=fortified;
    break;
   }
  }
  return observed;
 }

 void spawn_raid(){
  int slots=int(std::count_if(e.begin(),e.end(),[](const Entity& a){return a.hp<=0;}));
  if(!extra.mode)slots=std::min(slots,24-army(1));
  if(slots<=0){next_wave=time+3;return;}
  ++wave;next_wave=time+raid_interval();
  const bool defense=extra.mode, heavy=defense&&wave%5==0;
  const bool factory_ready=defense||ready_count(factory,1)>0;
  const auto intel=defense?RaidIntel{}:scout_raid();
  const bool aircraft=defense?wave>=6&&wave%3==0:(wave>=2||intel.air)&&ready_count(helipad,1)>0;
  int units=std::min(slots,defense?std::min(12,3+wave):std::min(7,2+wave/2+(chapter==2?1:chapter)));
  bool objective=!defense&&(chapter==1||chapter>=3);
  Vec base=base_position();
  int spawned=0;
  for(int j=0;j<units;++j){
   bool wing=j%3==1;
   RaidRole role=objective&&(wing||chapter==1)?RaidRole::intercept:wing&&wave>=2?RaidRole::flank:RaidRole::assault;
   Vec entry=wave_entry(wave+int(wing&&dual_raid_entry(wave)));
   int kind=j%3==0?tank:j%3==1?rifle:rocket;
   if(wing&&(intel.air||intel.armor))kind=rocket;
   if(j>0&&kind==tank&&!factory_ready&&chapter>=2)kind=rocket;
   bool siege=heavy&&j%3==0;
   siege|=j==units-1&&(defense?wave>=4:wave>=3||intel.fortified);
   if(factory_ready&&siege)kind=artillery;
   if(aircraft&&j==units-1)kind=rotorcraft;
   Vec start=raid_open_point(kind,{entry.x+(j%3)*.65f,entry.y+(j/3)*.65f},entry);
   if(!open_for(kind,start))start=raid_open_point(kind,wave_entry(wave+1),entry);
   if(!open_for(kind,start))continue;
   int id=add(kind,1,start);
   if(id<0)break;
   ++spawned;
   auto& a=e[id];
   float span=std::max(1.f,dist(entry,base));
   Vec gather{start.x+(base.x-entry.x)*2.f/span,start.y+(base.y-entry.y)*2.f/span};
   a.goal=raid_open_point(kind,gather,start);
   a.moving=true;
   a.target=int16_t(raid_order(role,true));
   if(defense&&extra.map_rules){
    constexpr uint8_t ranks[]={0,2,5,9};
    a.kills=ranks[std::min(3,(wave-1)/8)];
    a.hp*=1+std::min(.6f,(wave/10)*.08f);
   }
   if(heavy){a.hp*=1.25f;a.kills=std::max(uint8_t(2),a.kills);}
  }
  if(!spawned){--wave;next_wave=time+3;return;}
  say(heavy?"侦察：敌军重装部队正在集结！":dual_raid_entry(wave)?"侦察：敌军分两路集结！":"侦察：敌军正在集结！",6);
 }

 void enemy_orders(){
  int staging=0,ready=0;
  bool active=false;
  for(const auto& a:e)if(deployed(a)&&raider(a)){
   active=true;
   if(!rallying(a))continue;
   ++staging;
   if(distance_squared(a.p,a.goal)<.6f*.6f)++ready;
  }
  if(!active)return;
  // The next-wave clock also preserves the latest rally deadline in battle saves.
  bool launch=staging>0&&(ready==staging||time>=next_wave-raid_interval()+raid_rally_limit);
  Vec base=base_position();
  Vec goals[]={base,raid_objective(RaidRole::flank,base),raid_objective(RaidRole::intercept,base)};
  for(int i=0;i<MAX;++i){
   auto& a=e[i];
   if(!deployed(a)||!raider(a)||(rallying(a)&&!launch))continue;
   auto role=raid_role(a);
   Vec point=goals[int(role)];
   Vec desired{point.x+(i%3-1)*.9f,point.y+(i%2?-.7f:.7f)};
   Vec goal=raid_open_point(a.kind,desired,a.p);
   if(!rallying(a)&&distance_squared(goal,a.goal)<.7f*.7f)continue;
   a.target=int16_t(raid_order(role,false));
   a.goal=goal;a.step=a.p;a.stuck=0;
   a.moving=true;
  }
  if(launch)say("侦察：敌方编队开始推进！",5);
 }
