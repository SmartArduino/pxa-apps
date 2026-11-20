// SPDX-License-Identifier: MIT
// World members for the second act’s persistent mission objectives.
 int captured_nodes()const{return count(relay,0);}
 void frontier_tick(float dt){
  for(auto& node:e)if(node.hp>0&&node.kind==relay&&node.team==2){bool friendly=false,enemy=false;for(auto& a:e)if(deployed(a)&&land_combat(a.kind)&&dist(a.p,node.p)<2.7f){friendly|=a.team==0;enemy|=a.team==1;}if(friendly&&!enemy){node.stuck+=dt;if(node.stuck>=12){node.team=0;node.stuck=0;credits+=180;blast(node.p,6);power_update();say(chapter==4?"苏弥：中继站已接通，车队可以继续前进。":"苏弥：节点已接通。收到居民区的回传信号。",8);emit_sound(5);}}else if(!friendly)node.stuck=std::max(0.f,node.stuck-dt*.4f);}
  if(chapter==3){if(captured_nodes()>=2&&!mission.stage){mission.stage=1;mission.events|=1;say("林岚：已取得行车记录。摧毁军工厂，切断山区追兵。",9);}won=mission.stage&&!count(factory,1);if(count(relay,0)+count(relay,2)<2)defeated=true;}
  if(chapter==4) convoy_tick(dt);
  if(chapter==5){if(!mission.stage&&captured_nodes()>=3){mission.stage=1;mission.events|=1u<<3;say("墨沉：恢复城市供电，就会暴露所有人。你承担得起吗？",10);}if(mission.stage==1&&!count(power,1)){mission.stage=2;mission.events|=1u<<4;say("苏弥：居民电网已隔离！核心屏障解除，城市不会再停电。",10);}won=mission.stage==2&&!count(hq,1);if(count(relay,0)+count(relay,2)<3)defeated=true;}
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
   say("何舟：道路畅通。补给车跟上，装甲部队守住侧翼。",8);
  }
 }

 const char* active_objective()const{
  if(chapter==0)return "摧毁灰塔指挥中心";
  if(chapter==1)return "占领中继站，保护撤离 90 秒";
  if(chapter==2)return "击毁敌方电站，摧毁灰塔指挥中心";
  if(chapter==3)return mission.stage?"摧毁灰塔军工厂":"夺回两座黑松山通信节点";
  if(chapter==4){if(convoy_gate(1)&&convoy_gate(2))return "护送至少两辆补给车抵达出口";return "接通沿途电台，为补给车开路";}
  return mission.stage==0?"夺回三个节点，隔离居民电网":mission.stage==1?"摧毁两座军用电站":"屏障已解除，摧毁灰塔核心";
 }
