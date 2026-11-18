// SPDX-License-Identifier: MIT
// World members: authored large theatres and persistent mission objectives.
 void deposit(int x,int y,int width=3,int height=3){for(int yy=y;yy<y+height;++yy)for(int xx=x;xx<x+width;++xx){int c=yy*N+xx;terrain[c]=ground;ore[c]=180+random()%75;}}
 void lane(Vec a,Vec b,int width=2){int x=int(a.x),y=int(a.y);while(x!=int(b.x)||y!=int(b.y)){for(int j=0;j<width;++j){int xx=std::clamp(x+j,1,side-2),yy=std::clamp(y,1,side-2);terrain[yy*N+xx]=road;}if(x!=int(b.x))x+=x<int(b.x)?1:-1;else y+=y<int(b.y)?1:-1;}}
 void init_frontier(){
  side=chapter==3?40:chapter==4?44:48;rng=83711+chapter*7919;extra.map_rules=1;credits=1650;terrain.fill(water);next_wave=95;
  for(int y=1;y<side-1;++y)for(int x=1;x<side-1;++x){unsigned v=random();terrain[y*N+x]=v%9<2?woodland:ground;}
  add(hq,0,{5.5f,8.5f});add(power,0,{3.5f,7});add(power,0,{3,4});add(refinery,0,{4,11});add(barracks,0,{8,8});add(factory,0,{7,5});add(harvester,0,{5,12.5f});add(tank,0,{8.5f,11});add(tank,0,{9.5f,12});add(rifle,0,{8,13});add(rocket,0,{10.5f,11.5f});add(artillery,0,{9.5f,9.5f});add(turret,0,{10,7});
  deposit(3,14);deposit(12,5);deposit(side/2+3,side/2-5,4,3);deposit(side-10,side-11,4,3);
  lane({2,10},{float(side-7),10});lane({10,4},{10,float(side-6)});lane({10,18},{float(side-7),18});lane({float(side-10),10},{float(side-10),float(side-6)});
  // A continuous ridge leaves three broad, predictable tactical approaches.
  int divide=side/2;for(int y=2;y<side-2;++y){if((y>=8&&y<=12)||(y>=17&&y<=21)||(y>=side-10&&y<=side-6))continue;for(int x=divide;x<divide+2;++x){terrain[y*N+x]=chapter==3?ridge:water;ore[y*N+x]=0;}}
  if(chapter!=3)for(int y:{10,18,side-8})for(int yy=y-1;yy<=y+1;++yy)for(int x=divide;x<divide+2;++x)terrain[yy*N+x]=bridge;
  add(hq,1,{float(side-6),float(side-7)});add(power,1,{float(side-9),float(side-9)});add(factory,1,{float(side-6),float(side-12)});add(turret,1,{float(side-12),float(side-10)});add(turret,1,{float(side-9),float(side-14)});add(tank,1,{float(side-13),float(side-13)});add(rocket,1,{float(side-14),float(side-11)});
  if(chapter==3){add(relay,2,{18,10});add(relay,2,{29,18});add(turret,1,{24,16});add(tank,1,{24,19});add(supply,2,{16,17});add(supply,2,{29,8});say("苏弥：黑松山的两座电台还在。别让它们再次沉默。",9);}
  if(chapter==4){for(int j=0;j<3;++j)mission.convoy[j]=int16_t(add(harvester,2,{11.f+j,13.f+j}));add(relay,2,{21,18});add(relay,2,{33,29});add(supply,2,{15,21});add(supply,2,{32,23});next_wave=75;say("何舟：三辆补给车已经发动。先打通中继站，再带大家离开。",9);}
  if(chapter==5){add(relay,2,{18,14});add(relay,2,{29,22});add(relay,2,{38,35});add(power,1,{39,29});add(turret,1,{34,23});add(turret,1,{39,36});add(artillery,1,{37,32});add(supply,2,{16,21});add(supply,2,{29,32});say("林岚：不再切断整座城市。夺回三个节点，把电网交还给居民。",9);}
  // Clear approaches at authored actors and route waypoints; no random terrain
  // may entomb a civilian truck or place a mission node in water/rock.
  for(auto& a:e)if(a.hp>0)for(int y=std::max(1,int(a.p.y)-2);y<=std::min(side-2,int(a.p.y)+2);++y)for(int x=std::max(1,int(a.p.x)-2);x<=std::min(side-2,int(a.p.x)+2);++x)if(terrain[y*N+x]==ridge||terrain[y*N+x]==water)terrain[y*N+x]=ground;
  reveal();power_update();
 }
 int captured_nodes()const{return count(relay,0);}
 void frontier_tick(float dt){
  for(auto& node:e)if(node.hp>0&&node.kind==relay&&node.team==2){bool friendly=false,enemy=false;for(auto& a:e)if(a.hp>0&&a.kind<harvester&&dist(a.p,node.p)<2.7f){friendly|=a.team==0;enemy|=a.team==1;}if(friendly&&!enemy){node.stuck+=dt;if(node.stuck>=12){node.team=0;node.stuck=0;credits+=180;blast(node.p,6);power_update();say(chapter==4?"苏弥：中继站已接通，车队可以继续前进。":"苏弥：节点已接通。收到居民区的回传信号。",8);emit_sound(5);}}else if(!friendly)node.stuck=std::max(0.f,node.stuck-dt*.4f);}
  if(chapter==3){if(captured_nodes()>=2&&!mission.stage){mission.stage=1;mission.events|=1;say("林岚：电台记录指向东南军工厂。摧毁它，切断山区追兵。",9);}won=mission.stage&&!count(factory,1);if(count(relay,0)+count(relay,2)<2)defeated=true;}
  if(chapter==4){int alive=0,delivered=0;for(int j=0;j<3;++j){if(mission.delivered&(1<<j)){++delivered;continue;}int id=mission.convoy[j];if(id<0||e[id].hp<=0)continue;++alive;auto& a=e[id];unsigned step=mission.waypoint[j];const Vec route[]={{17,18},{26,18},{35,29},{37,40}};if((step==1&&captured_nodes()<1)||(step==2&&captured_nodes()<2)){a.moving=false;continue;}Vec target=route[std::min(3u,step)];if(dist(a.p,target)<1.1f){if(step==3){mission.delivered|=1<<j;a.hp=0;++delivered;--alive;credits+=200;blast(target,6);}else ++mission.waypoint[j];}else{a.goal=target;a.moving=true;move(id,dt);}}won=delivered>=2;defeated|=alive+delivered<2;if(captured_nodes()>mission.stage){mission.stage=uint8_t(captured_nodes());mission.events|=1u<<(mission.stage==1?1:2);say("何舟：道路畅通。补给车跟上，装甲部队守住侧翼。",8);}if(count(relay,0)+count(relay,2)<2)defeated=true;}
  if(chapter==5){if(!mission.stage&&captured_nodes()>=3){mission.stage=1;mission.events|=1u<<3;say("墨沉：恢复城市供电，就会暴露所有人。你承担得起吗？",10);}if(mission.stage==1&&!count(power,1)){mission.stage=2;mission.events|=1u<<4;say("苏弥：居民电网已隔离！核心屏障解除，城市不会再停电。",10);}won=mission.stage==2&&!count(hq,1);if(count(relay,0)+count(relay,2)<3)defeated=true;}
 }
 const char* active_objective()const{
  if(chapter==0)return "摧毁东南方的灰塔指挥中心";
  if(chapter==1)return "占领中继站，保护撤离 90 秒";
  if(chapter==2)return "击毁敌方电站，摧毁灰塔指挥中心";
  if(chapter==3)return mission.stage?"摧毁东南军工厂":"夺回两座黑松山通信节点";
  if(chapter==4)return "接通沿途电台，护送至少两辆补给车";
  return mission.stage==0?"夺回三个节点，隔离居民电网":mission.stage==1?"摧毁两座军用电站":"屏障已解除，摧毁灰塔核心";
 }
