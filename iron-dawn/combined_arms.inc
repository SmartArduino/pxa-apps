// SPDX-License-Identifier: MIT
// Aircraft use the whole interior of the map. Boats use the channel and its
// bridge cells, which represent clear water under the deck.

 bool waterway_cell(int c)const{return terrain[c]==water||terrain[c]==bridge;}

 bool open_for(int kind,Vec p,int ignore=-1)const{
  if(kind!=patrol_boat&&kind!=rotorcraft)return open(p,ignore);
  if(p.x<1||p.y<1||p.x>=side-1||p.y>=side-1)return false;
  return kind==rotorcraft||waterway_cell(cell(p));
 }

 bool has_shore_water()const{
  for(int y=1;y<side-1;++y)
   for(int x=1;x<side-1;++x)if(terrain[y*N+x]==water)return true;
  return false;
 }

 Vec shore_exit(Vec bank)const{
  Vec found=bank;float best=6.26f;
  int x=int(bank.x),y=int(bank.y);
  for(int yy=std::max(1,y-3);yy<=std::min(side-2,y+3);++yy)
   for(int xx=std::max(1,x-3);xx<=std::min(side-2,x+3);++xx){
    int c=yy*N+xx;
    if(terrain[c]!=water)continue;
    auto point=center(c);
    float d=distance_squared(bank,point);
    if(d<best){best=d;found=point;}
   }
  return found;
 }

 int selected_commander()const{
  for(int i=0;i<MAX;++i)if(e[i].hp>0&&e[i].team==0&&e[i].kind==commander&&e[i].selected)return i;
  return -1;
 }

 bool commander_support(){
  int id=selected_commander();
  if(id<0){say("请先选中林岚");return false;}
  if(mission.support_left>0){say("整备指令冷却中");return false;}
  auto position=e[id].p;
  bool repaired=false;
  for(auto& a:e){
   if(a.hp<=0||a.team!=0||!combat_kind(a.kind)||a.kind==rotorcraft||a.hp>=specs[a.kind].hp)continue;
   if(distance_squared(a.p,position)>support_radius*support_radius)continue;
   a.hp=std::min(float(specs[a.kind].hp),a.hp+specs[a.kind].hp*.24f);
   blast(a.p,3);repaired=true;
  }
  if(!repaired){say("整备范围内没有受损部队");return false;}
  mission.support_left=support_cooldown;
  say("林岚：战地整备已完成。",7);emit_sound(5);
  return true;
 }
