// SPDX-License-Identifier: MIT
// A live combat unit belongs to at most one squad.

 bool squad_unit(int id) const {
  const auto& a=e[id];
  return a.hp>0&&a.team==0&&combat_kind(a.kind);
 }

 int squad_size(unsigned number) const {
  if(number<1||number>squad_count)return 0;
  int count=0;
  for(int i=0;i<MAX;++i)count+=squads[i]==number&&squad_unit(i);
  return count;
 }

 bool has_squad_selection() const {
  for(int i=0;i<MAX;++i)if(e[i].selected&&squad_unit(i))return true;
  return false;
 }

 bool assign_squad(unsigned number) {
  if(number<1||number>squad_count||!has_squad_selection())return false;
  for(int i=0;i<MAX;++i){
   if(e[i].selected&&squad_unit(i))squads[i]=uint8_t(number);
   else if(squads[i]==number)squads[i]=0;
  }
  char message[96];
  std::snprintf(message,sizeof message,"已编入 %u 队，共 %d 个单位",number,squad_size(number));
  say(message);
  return true;
 }

 bool squad_selected(unsigned number) const {
  if(!squad_size(number))return false;
  for(int i=0;i<MAX;++i){
   bool member=squads[i]==number&&squad_unit(i);
   if((e[i].hp>0&&e[i].selected)!=member)return false;
  }
  return true;
 }

 bool recall_squad(unsigned number) {
  if(!squad_size(number))return false;
  for(int i=0;i<MAX;++i)e[i].selected=squads[i]==number&&squad_unit(i);
  char message[96];
  std::snprintf(message,sizeof message,"已选 %u 队，再召回可定位",number);
  say(message);
  return true;
 }

 Vec squad_center(unsigned number) const {
  Vec sum{};int count=0;
  for(int i=0;i<MAX;++i)if(squads[i]==number&&squad_unit(i)){
   sum.x+=e[i].p.x;sum.y+=e[i].p.y;++count;
  }
  return count?Vec{sum.x/count,sum.y/count}:base_position();
 }
