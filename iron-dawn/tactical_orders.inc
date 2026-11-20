// SPDX-License-Identifier: MIT
// Guards use the existing order field. Convoy escorts transfer when their
// truck is lost or delivered, while mine guards hold where the truck was lost.

 bool escortable(int id) const {
  if(id<0||id>=MAX)return false;
  const auto& a=e[id];
  if(a.hp<=0||a.kind!=harvester)return false;
  if(a.team==0)return true;
  if(a.team!=2||extra.mode||chapter!=4)return false;
  for(int j=0;j<3;++j)if(mission.convoy[j]==id&&!(mission.delivered&(1u<<j)))return true;
  return false;
 }

 bool order_escort(int truck){
  if(!escortable(truck)){say("请选择己方矿车或补给车");return false;}
  bool ordered=false;
  for(int id=0;id<MAX;++id){
   auto& guard=e[id];
   if(!guard.selected||!squad_unit(id)||naval_kind(guard.kind))continue;
   guard.target=int16_t(escort_order_base-truck);
   guard.goal=e[truck].p;guard.step=guard.p;guard.stuck=0;guard.moving=true;
   ordered=true;
  }
  say(ordered?(e[truck].team==2?"护卫队跟随补给车":"护卫队跟随采矿车"):"请先选择作战部队");
  if(ordered)emit_sound(1);
  return ordered;
 }

 bool order_retreat(Vec position){
  int count=0;
  for(int id=0;id<MAX;++id){
   auto& unit=e[id];
   if(!unit.selected||!squad_unit(id)||!open_for(unit.kind,position))continue;
   unit.goal=formation_goal(position,count++,unit.kind);unit.step=unit.p;unit.stuck=0;
   unit.moving=true;unit.target=retreat_order;
  }
  say(count?"收到，部队开始撤离":"请选择该编队可通行的撤退地点");
  if(count)emit_sound(1);
  return count>0;
 }

 void release_escort(Entity& unit){
  unit.target=-1;
  unit.goal=unit.step=unit.p;
  unit.stuck=0;
  unit.moving=false;
 }

 void finish_escort_target(int truck){
  bool convoy=e[truck].team==2&&chapter==4&&!extra.mode&&e[truck].kind==harvester;
  for(auto& guard:e)if(guard.hp>0&&guard.team==0&&combat_kind(guard.kind)&&escort_id(guard)==truck){
   int next=-1;
   if(convoy){
    float best=0;
    for(int candidate:mission.convoy)if(candidate!=truck&&escortable(candidate)){
     float d=distance_squared(guard.p,e[candidate].p);
     if(next<0||d<best){next=candidate;best=d;}
    }
   }
   if(next<0)release_escort(guard);
   else{
    guard.target=int16_t(escort_order_base-next);
    guard.goal=e[next].p;guard.step=guard.p;guard.stuck=0;guard.moving=true;
   }
  }
 }

 void update_escort(Entity& guard,float dt){
  int id=escort_id(guard);
  if(!escortable(id)){release_escort(guard);return;}
  float radius=guard.kind==artillery?2.8f:1.8f;
  float d=distance_squared(guard.p,e[id].p);
  if(d<radius*radius){
   guard.moving=false;guard.step=guard.p;guard.stuck=0;return;
  }
  float resume=radius+.75f;
  if(guard.moving||d>resume*resume){
   if(!guard.moving){
    if(guard.stuck>2.f&&distance_squared(guard.goal,e[id].p)<.25f){
     guard.stuck+=dt;
     if(guard.stuck<3.f)return;
    }
    guard.step=guard.p;guard.stuck=0;
   }
   guard.goal=e[id].p;
   guard.moving=true;
  }
 }
