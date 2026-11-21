// SPDX-License-Identifier: MIT
// The shore and the bridge deck are separate surfaces. A boat can pass under
// a bridge, but its passengers can only take the ramp onto dry banks.

 bool transport_exists(int id)const{
  return id>=0&&id<MAX&&e[id].hp>0&&e[id].team==0&&e[id].kind==landing_craft;
 }

 int transport_load(int id,bool include_approaching=false)const{
  if(id<0||id>=MAX)return 0;
  int total=0;
  for(const auto& a:e)if(a.hp>0)
   total+=passenger_ship(a)==id||(include_approaching&&boarding_ship(a)==id);
  return total;
 }

 int selected_loaded_transport()const{
  for(int i=0;i<MAX;++i)if(e[i].selected&&transport_exists(i)&&transport_load(i)>0)return i;
  return -1;
 }

 bool may_board()const{
  if(!count(landing_craft))return false;
  for(const auto& a:e)if(deployed(a)&&a.team==0&&a.selected&&land_combat(a.kind))return true;
  return false;
 }

 bool bank_cell(Vec p,Vec boat)const{
  return open(p)&&terrain[cell(p)]!=bridge&&distance_squared(p,boat)<=boarding_range*boarding_range;
 }

 Vec boarding_bank(Vec from,Vec boat)const{
  Vec nearest=boat;
  float best=1e9f;
  int x=int(boat.x),y=int(boat.y);
  for(int yy=std::max(1,y-2);yy<=std::min(side-2,y+2);++yy)
   for(int xx=std::max(1,x-2);xx<=std::min(side-2,x+2);++xx){
    Vec p=center(yy*N+xx);
    if(!bank_cell(p,boat))continue;
    float d=distance_squared(from,p);
    if(d<best){nearest=p;best=d;}
   }
  return nearest;
 }

 bool order_board(int id){
  if(!transport_exists(id)){say("请点选己方渡运艇");return false;}
  const auto& boat=e[id];
  if(boat.moving){say("先让渡运艇靠岸停止");return false;}
  if(!bank_cell(boarding_bank(boat.p,boat.p),boat.p)){say("渡运艇须靠岸");return false;}
  int available=transport_capacity-transport_load(id,true);
  int ordered=0;
  for(auto& a:e)if(deployed(a)&&a.team==0&&a.selected&&land_combat(a.kind)&&boarding_ship(a)!=id){
   if(available<=0)break;
   a.goal=boarding_bank(a.p,boat.p);
   a.step=a.p;a.stuck=0;a.moving=true;a.target=int16_t(board_order_base-id);
   --available;++ordered;
  }
  say(ordered?"小队正在赶往渡运艇":available==0?"渡运艇席位已满":"请选择地面作战单位");
  if(ordered)emit_sound(1);
  return ordered>0;
 }

 void cancel_boarding(Entity& a){
  a.target=-1;a.goal=a.step=a.p;a.moving=false;a.stuck=0;
 }

 void update_boarding(int id,float dt){
  auto& a=e[id];int boat=boarding_ship(a);
  if(!transport_exists(boat)||e[boat].moving){cancel_boarding(a);return;}
  // Movement stops just short of the ramp. Check its tile against the boat's
  // reach so troops approaching from inland can board at the edge of range.
  if(distance_squared(a.p,a.goal)<.28f*.28f&&bank_cell(a.goal,e[boat].p)&&open(a.p)&&terrain[cell(a.p)]!=bridge){
   a.target=int16_t(passenger_order_base-boat);
   a.p=a.goal=a.step=e[boat].p;a.moving=false;a.selected=false;a.stuck=0;
   blast(a.p,3);
   say("部队已登船");
   return;
  }
  if(a.moving)move(id,dt);
  if(!a.moving)cancel_boarding(a);
 }

 bool order_land(int id,Vec p){
  if(!transport_exists(id)||!transport_load(id)){say("请选择载有部队的渡运艇");return false;}
  auto& boat=e[id];
  p=center(cell(p));
  if(boat.moving||!seen[cell(p)]||!bank_cell(p,boat.p)){
   say("先靠岸停止，再点附近已探索的空地");return false;
  }
  constexpr Vec offsets[]={{-.18f,-.18f},{.18f,-.18f},{-.18f,.18f},{.18f,.18f}};
  int placed=0;
  for(auto& a:e)if(a.hp>0&&passenger_ship(a)==id&&placed<transport_capacity){
   Vec spawn{p.x+offsets[placed].x,p.y+offsets[placed].y};
   a.p=open(spawn)?spawn:p;
   cancel_boarding(a);
   ++placed;
  }
  say("渡运小队已登陆，等待下一道指令");
  emit_sound(1);
  return true;
 }

 void remove_transport_passengers(int id){
  int casualties=0;
  for(int i=0;i<MAX;++i)if(e[i].hp>0){
   auto& a=e[i];
   if(boarding_ship(a)==id)cancel_boarding(a);
   else if(passenger_ship(a)==id){
    cancel_boarding(a);a.hp=0;squads[i]=0;++casualties;
   }
  }
  if(casualties){lost+=casualties;say("渡运艇沉没，随船部队损失");}
 }

 void sync_passengers(){
  for(auto& a:e)if(a.hp>0){
   int ship=passenger_ship(a);
   if(ship<0)continue;
   if(transport_exists(ship))a.p=a.goal=a.step=e[ship].p;
   else remove_transport_passengers(ship);
  }
 }
