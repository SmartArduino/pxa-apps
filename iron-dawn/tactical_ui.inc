// SPDX-License-Identifier: MIT
// Arming a tactical command changes the next map tap. The issued order lives in World.

 const char* tactical_hint()const{
  if(tactical==TacticalMode::escort)return "护卫：点击矿车或补给车";
  if(tactical==TacticalMode::retreat)return "撤退：点击要撤往的空地";
  return nullptr;
 }

 void arm_tactical(TacticalMode mode){
  if(tactical==mode){tactical=TacticalMode::none;world.say("已取消待发指令");return;}
  if(!world.has_squad_selection()){world.say("请先选择作战部队");return;}
  placing=-1;tactical=mode;last_selected_id=-1;world.say(tactical_hint());
 }

 int escort_vehicle_at(int x,int y)const{
  int best=-1;
  float limit=18.f*18.f*field.zoom*field.zoom;
  for(int i=0;i<MAX;++i)if(world.escortable(i)){
   const auto& truck=world.e[i];
   if(truck.team==2&&!world.fog[cell(truck.p)])continue;
   auto p=field.project(truck.p,10);
   float d=distance_squared({float(x),float(y)},{p.x,p.y});
   if(d<limit){best=i;limit=d;}
  }
  return best;
 }

 void tactical_tap(int x,int y,Vec position){
  if(tactical==TacticalMode::escort){
   int vehicle=escort_vehicle_at(x,y);
   if(!world.order_escort(vehicle))return;
   char message[80];std::snprintf(message,sizeof message,"Iron Dawn escort order truck=%d",vehicle);log(message);
  }else{
   if(!world.order_retreat(position))return;
   log("Iron Dawn retreat order");
  }
  tactical=TacticalMode::none;
  sounds.respond(world,UnitCue::move);
  save();
 }

 void tactical_buttons(Raster& r,Rect escort,Rect retreat){
  bool selected=world.has_squad_selection();
  button(r,escort,tactic_escort,"护卫",tactical==TacticalMode::escort,selected);
  button(r,retreat,tactic_retreat,"撤退",tactical==TacticalMode::retreat,selected);
 }
