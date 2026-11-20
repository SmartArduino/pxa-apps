// SPDX-License-Identifier: MIT
// App members for selecting production facilities and deploying finished buildings.

 int production_card(bool structures,int index)const{
  if(structures){
   if(!combined_page)return power+index;
   return index<3?shipyard+index:-1;
  }
  int id=world.selected_producer();
  if(id<0)return -1;
  switch(world.e[id].kind){
   case hq:return index==0?commander:-1;
   case shipyard:return index==0?patrol_boat:-1;
   case helipad:return index==0?rotorcraft:-1;
   case barracks:return index<2?rifle+index:-1;
   case factory:return index<3?tank+index:-1;
   default:return -1;
  }
 }

 const char* short_title(int kind)const{
  constexpr const char* titles[]={"游骑","破甲","坦克","火炮","矿车","指挥中心","电站","矿厂","兵营","工厂","炮塔","中继站","补给",
     "巡逻艇","旋翼机","林岚","船坞","机坪","防空塔"};
  return titles[kind];
 }

 const char* production_label()const{
  int id=world.selected_producer();
  return id<0?"点选生产设施":specs[world.e[id].kind].name;
 }

 const char* construction_label()const{
  return world.build_ready()?"待部署":world.build_active()?"施工中":"建造";
 }

 struct CardState {bool enabled=false,active=false;int queued=0;float progress=0;const char* text=nullptr;};
 CardState production_state(int k)const{
  CardState state;
  if(construction_kind(k)){
   state.active=world.build_active()&&world.construction.kind==k;
   state.enabled=state.active||(world.unlocked(k)&&!world.build_active());
   if(state.active){
    state.progress=1-world.construction.left/world.construction.total;
    state.text=world.build_ready()?"可部署":"施工中";
   }
  }else{
   state.enabled=true;
   int facility=world.selected_producer();
   for(int i=0;i<world.job_count;++i)
    state.queued+=world.jobs[i].kind==k&&(!world.jobs[i].producer||world.jobs[i].producer==facility+1);
   state.active=state.queued&&world.jobs[0].kind==k&&(!world.jobs[0].producer||world.jobs[0].producer==facility+1);
   if(state.active)state.progress=1-world.jobs[0].left/world.jobs[0].total;
  }
  if(!state.enabled)state.text="锁定";
  return state;
 }

 void production_picture(Raster& r,int k,Rect area,const CardState& state){
  if(k>=power&&k<=turret&&state.active&&!world.build_ready()){
   unsigned stage=std::min(3u,unsigned(state.progress*4));
   Sprites::fit(r,art::construction_first+(k-power)*4+stage,area);
  }else Skin::picture(r,k,area,state.enabled?0:4);
 }

 void production_action(int k){
  if(construction_kind(k)){
   if(world.build_active()&&world.construction.kind==k){
    if(world.build_ready()){
     placing=k;pointer_world=world.base_position();dock.close();
     world.say("点选基地附近的空地部署，取消可保留建筑",8);
    }else world.say("施工进行中，完工后即可部署");
   }else if(world.start_construction(k))save();
  }else{
   int id=world.selected_producer();
   if(id>=0&&world.queue(k,id))save();
   else if(id<0)world.say("点选兵营或工厂");
  }
  report();
 }

 int building_at(int x,int y)const{
  int best=-1;float depth=-1;
  for(int i=0;i<MAX;++i){
   auto& a=world.e[i];
   if(a.hp<=0||unit_kind(a.kind)||(a.team!=0&&!world.fog[cell(a.p)]))continue;
   auto point=field.project(a.p);
   auto& f=art::frames[Sprites::building_frame(a.kind)];
   float z=field.zoom,left=point.x+(int(f.bx)-f.ox)*z,top=point.y+(int(f.by)-f.oy)*z;
   if(x>=left&&x<left+f.bw*z&&y>=top&&y<top+f.bh*z&&a.p.x+a.p.y>=depth){
    depth=a.p.x+a.p.y;best=i;
   }
  }
  return best;
 }

 int combined_unit_at(int x,int y)const{
  int found=-1;float best=1;
  for(int i=0;i<MAX;++i){
   auto& a=world.e[i];
   if(a.hp<=0||(a.kind!=rotorcraft&&a.kind!=patrol_boat&&a.kind!=commander)
      ||(a.team!=0&&!world.fog[cell(a.p)]))continue;
   int height=a.kind==rotorcraft?craft_altitude+13:a.kind==commander?18:12;
   float radius=(a.kind==commander?13.f:18.f)*field.zoom;
   auto p=field.project(a.p,height);
   float d=distance_squared({float(x),float(y)},{p.x,p.y})/(radius*radius);
   if(d<best){best=d;found=i;}
  }
  return found;
 }

 void support_button(Raster& r,Rect area){
  if(world.selected_commander()<0)return;
  char label[32];
  if(world.mission.support_left>0)
   std::snprintf(label,sizeof label,"%u秒",unsigned(std::ceil(world.mission.support_left)));
  else std::snprintf(label,sizeof label,"整备");
  button(r,area,hero_support,label,false,world.mission.support_left<=0);
 }

 void show_build_grid(Raster& r){
  if(!construction_kind(placing))return;
  auto clip=r.clip;r.clip=battlefield();
  Vec p=world.build_site(placing,pointer_world);
  bool clear=world.can_place(placing,p);
  auto color=clear?cyan:red;
  float extent=defence_kind(placing)?.5f:1.f;
  for(float d=-extent;d<=extent;d+=1){
   field.wire(r,{p.x-extent,p.y+d},{p.x+extent,p.y+d},0,color,2);
   field.wire(r,{p.x+d,p.y-extent},{p.x+d,p.y+extent},0,color,2);
  }
  auto q=field.project(p);
  r.text(q.x,q.y-32*field.zoom,clear?"可以部署":"无法部署",color,12,1);
  r.clip=clip;
 }
