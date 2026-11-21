// SPDX-License-Identifier: MIT
// World members for the second act’s persistent mission objectives.
 int relay_at(Vec site,int team)const{
  int id=nearest(site,relay,team);
  return id>=0&&distance_squared(site,e[id].p)<.01f?id:-1;
 }

 int captured_nodes()const{return count(relay,0);}
 struct NodePresence {bool friendly=false,enemy=false;};
 NodePresence node_presence(Vec p)const{
  NodePresence present;
  for(const auto& a:e)if(deployed(a)&&land_combat(a.kind)&&distance_squared(a.p,p)<2.7f*2.7f){
   present.friendly|=a.team==0;
   present.enemy|=a.team==1;
  }
  return present;
 }
 bool capture_node(Entity& node,float dt){
  auto present=node_presence(node.p);
  if(!present.friendly)node.stuck=std::max(0.f,node.stuck-dt*.4f);
  if(!present.friendly||present.enemy)return false;
  node.stuck+=dt;
  if(node.stuck<12.f)return false;
  node.team=0;node.stuck=0;credits+=180;blast(node.p,6);power_update();emit_sound(5);
  return true;
 }
 void frontier_tick(float dt){
  for(auto& node:e)if(node.hp>0&&node.kind==relay&&node.team==2&&capture_node(node,dt))
   say(chapter==4?"苏弥：电台接通了，卡车可以继续走。":"苏弥：收到信号，这一座已经接通。",8);
  if(chapter==3){if(captured_nodes()>=2&&!mission.stage){mission.stage=1;mission.events|=1;say("林岚：找到车队的位置了。先打东边的军工厂。",9);}won=mission.stage&&!count(factory,1);if(count(relay,0)+count(relay,2)<2)defeated=true;}
  if(chapter==4) convoy_tick(dt);
  if(chapter==5){if(!mission.stage&&captured_nodes()>=3){mission.stage=1;mission.events|=1u<<3;say("墨沉：居民线路已经分开。现在可以去关军用电站了。",10);}if(mission.stage==1&&!count(power,1)){mission.stage=2;mission.events|=1u<<4;say("苏弥：医院还有电，核心没了屏障。可以进攻了。",10);}won=mission.stage==2&&!count(hq,1);if(count(relay,0)+count(relay,2)<3)defeated=true;}
 }
 void convoy_tick(float dt) {
  int alive=0, delivered=0;
  for (int j=0;j<3;++j) {
   if (mission.delivered&(1<<j)) {++delivered;continue;}
   int id=mission.convoy[j];
   if (!active_convoy(id)) continue;
   ++alive;
   auto& truck=e[id];
   unsigned step=mission.waypoint[j];
   if (!convoy_gate(step)) {truck.moving=false;continue;}
   Vec target=convoy_stop(step);
   if (dist(truck.p,target)<1.1f) {
    if (step==3) {
     mission.delivered|=1<<j;truck.hp=0;finish_escort_target(id);++delivered;--alive;credits+=200;blast(target,6);
    } else ++mission.waypoint[j];
   } else {
    if(dist(truck.goal,target)>.01f)truck.step=truck.p;
    truck.goal=target;truck.moving=true;move(id,dt);
   }
  }
  won=delivered>=2;
  defeated|=alive+delivered<2||count(relay,0)+count(relay,2)<2;
  unsigned stage=convoy_gate(1)?(convoy_gate(2)?2:1):0;
  if (stage>mission.stage) {
   for (unsigned i=mission.stage;i<stage;++i)mission.events|=1u<<(i+1);
   mission.stage=uint8_t(stage);
   say("何舟：收到。车队继续走，请派装甲跟着我们。",8);
  }
 }

 const char* active_objective()const{
  if(chapter==0)return "摧毁东北的灰塔指挥堡";
  if(chapter==1)return "占领西岸中继站，守住 90 秒";
  if(chapter==2)return "摧毁两座电站，再打南边的指挥堡";
  if(chapter==3)return mission.stage?"摧毁东边的军工厂":"占领山口和矿场的两座电台";
  if(chapter==4){if(convoy_gate(1)&&convoy_gate(2))return "护送至少两辆补给车到避难所";return convoy_gate(1)?"占领西岸引导台，让卡车过桥":"占领货场电台，让卡车出发";}
  if(chapter==harbor_chapter){
   if(mission.stage==harbor_beacon)return "守住信标并维持供电 45 秒";
   if(mission.stage==harbor_shutdown)return "守住停机站，摧毁西南控制中心";
   return "占领岛上信标或南边的停机站";
  }
  if(chapter==lighthouse_chapter){
   if(mission.stage==restore_grid)return "占领岛上的配电站";
   if(mission.stage==capture_lighthouse)return "占领岛北的灯塔";
   return "守住灯塔并维持供电 35 秒";
  }
  return mission.stage==0?"占领三座调度站，保住居民用电":mission.stage==1?"摧毁两座军用电站":"摧毁西北的灰塔核心";
 }
