// SPDX-License-Identifier: MIT
// World members shared by the selection UI, training and deployment.

 bool can_train_at(int k,int id) const {
  if(!unit_kind(k)||id<0||id>=MAX)return false;
  const auto& a=e[id];
  return a.hp>0&&a.team==0&&a.progress<=0&&a.kind==producer_for(k);
 }

 int selected_producer() const {
  for(int i=0;i<MAX;++i){
   auto& a=e[i];
   if(a.selected&&a.team==0&&a.hp>0&&a.progress<=0&&production_building(a.kind))return i;
  }
  return -1;
 }

 bool queue(int k,int producer=-1){
  if(!unit_kind(k))return false;
  if(producer>=0&&!can_train_at(k,producer)){say("请先选中对应的生产建筑");return false;}
  if(auto reason=blocked(k)){say(reason);return false;}
  credits-=specs[k].cost;
  jobs[job_count++]={uint8_t(k),uint8_t(producer+1),specs[k].build,specs[k].build};
  say("生产指令已确认");emit_sound(1);return true;
 }

 void remove_job(int index){
  for(int j=index+1;j<job_count;++j)jobs[j-1]=jobs[j];
  jobs[--job_count]={};
 }

 void cancel_job(){
  if(!job_count)return;
  credits+=specs[jobs[job_count-1].kind].cost;
  remove_job(job_count-1);
  say("已取消末项，资金退回");
 }

 void refund_destroyed_facility(int id){
  for(int i=job_count-1;i>=0;--i)if(jobs[i].producer==id+1){
   credits+=specs[jobs[i].kind].cost;
   remove_job(i);
  }
 }

 Vec production_exit(Vec p) const {
  constexpr Vec exits[]={{1.6f,1.6f},{1.6f,-1.6f},{-1.6f,1.6f},{-1.6f,-1.6f},{2.2f,0},{-2.2f,0},{0,2.2f},{0,-2.2f}};
  for(auto d:exits){Vec exit{p.x+d.x,p.y+d.y};if(open(exit))return exit;}
  return p;
 }

 void training_tick(float dt){
  if(!job_count)return;
  auto& job=jobs[0];
  int k=job.kind;
  int home=job.producer?job.producer-1:nearest(base_position(),producer_for(k),0);
  if(job.producer&&!can_train_at(k,home)){
   credits+=specs[k].cost;remove_job(0);say("生产设施已损毁，资金已退回");return;
  }
  job.left=std::max(0.f,job.left-dt*(power_net<0?.45f:1.f));
  if(job.left>0)return;
  if(home>=0){
   Vec base=e[home].p;
   Vec pos=naval_kind(k)?shore_exit(base):k==rotorcraft?base:production_exit(base);
   if(!open_for(k,pos))return; // Keep the unit ready until there is a usable exit.
   int id=add(k,0,pos);
   if(id>=0){
    blast(pos,3);
    if(e[home].moving&&open_for(k,e[home].goal)){
     e[id].goal=e[home].goal;e[id].step=pos;e[id].moving=true;
    }
    say("新部队已就绪");emit_sound(11);
    remove_job(0);return;
   }
  }
  credits+=specs[k].cost;remove_job(0);say("生产无法完成，资金已退回");
 }

 bool build_active() const {return construction_kind(construction.kind);}
 bool build_ready() const {return build_active()&&construction.left<=0;}

 const char* build_blocked(int k) const {
  if(!construction_kind(k))return "无法建造该设施";
  if(!ready_count(hq))return "需要指挥中心";
  if(k==shipyard&&!has_shore_water())return "当前战场没有可航水面";
  if(build_active())return "请先部署或退回当前建筑";
  if(!unlocked(k)){
   if(k==factory)return "需要兵营和精炼厂";
   if(k==shipyard)return "需要精炼厂";
   if(k==helipad)return "需要装甲工厂";
   return "需要步兵营";
  }
  if(credits<specs[k].cost)return "矿石不足，等待采矿车返航";
  return nullptr;
 }

 bool start_construction(int k){
  if(auto reason=build_blocked(k)){say(reason);return false;}
  credits-=specs[k].cost;
  construction={uint8_t(k),0,specs[k].build,specs[k].build};
  say("施工开始，完工后选择部署地点");emit_sound(10);return true;
 }

 void construction_tick(float dt){
  if(!build_active()||build_ready()||!ready_count(hq))return;
  construction.left=std::max(0.f,construction.left-dt*(power_net<0?.5f:1.f));
  if(build_ready()){say("建筑就绪，点击卡片即可部署");emit_sound(11);}
 }

 bool cancel_construction(){
  if(!build_active())return false;
  credits+=specs[construction.kind].cost;construction={};
  say("已撤回建筑，资金退回");return true;
 }

 Vec build_site(int k,Vec p) const {
  float offset=defence_kind(k)?.5f:0.f;
  return {std::floor(p.x)+offset,std::floor(p.y)+offset};
 }

 bool site_clear(Vec p) const {
  int c=cell(p);
  if(!open(p)||!seen[c]||ore[c]||terrain[c]==bridge||terrain[c]==railbed)return false;
  for(auto& a:e)if(deployed(a)&&unit_kind(a.kind)&&a.kind!=rotorcraft&&cell(a.p)==c)return false;
  return true;
 }

 bool can_place(int k,Vec p) const {
  if(!construction_kind(k))return false;
  p=build_site(k,p);
  if(k==shipyard&&distance_squared(p,shore_exit(p))<.01f)return false;
  float extent=defence_kind(k)?.5f:1.f;
  for(float y=p.y-extent+.5f;y<p.y+extent;y+=1)
   for(float x=p.x-extent+.5f;x<p.x+extent;x+=1)
    if(!site_clear({x,y}))return false;
  for(auto& a:e)
   if(a.hp>0&&a.team==0&&(a.kind==hq||construction_kind(a.kind)||a.kind==relay)&&dist(a.p,p)<6.f)return true;
  return false;
 }

 bool place(int k,Vec p){
  if(!build_ready()||construction.kind!=k||!ready_count(hq))return false;
  if(!can_place(k,p)){say(k==shipyard?"船坞须靠近水道及己方基地":"请选基地附近已探索的空地");return false;}
  p=build_site(k,p);
  Vec exit=production_exit(p);
  if(k==refinery){
   int slots=0;for(auto& a:e)slots+=a.hp<=0;
   if(slots<2||dist(exit,p)<.01f){say("精炼厂需要空位供矿车驶出");return false;}
  }
  if(add(k,0,p)<0){say("实体数量已达上限");return false;}
  construction={};
  if(k==refinery)add(harvester,0,exit);
  power_update();blast(p,3);emit_sound(11);say("建筑已部署");
  return true;
 }
