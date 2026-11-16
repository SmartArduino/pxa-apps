// SPDX-License-Identifier: MIT
#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
namespace iron {
constexpr int N=28,MAX=100;
struct Vec {float x=0,y=0;};
inline float dist(Vec a,Vec b){return std::hypot(a.x-b.x,a.y-b.y);}
inline int cell(Vec p){return std::clamp(int(p.y),0,N-1)*N+std::clamp(int(p.x),0,N-1);}
inline Vec center(int c){return {float(c%N)+.5f,float(c/N)+.5f};}
enum Kind:uint8_t {rifle,rocket,tank,artillery,harvester,hq,power,refinery,barracks,factory,turret,relay,supply};
struct Spec {const char* name;int cost,hp;float speed,range,damage,reload,build;int supply;};
inline constexpr Spec specs[]={
 {"游骑兵",90,100,1.25f,3.5f,12,.65f,3,0},
 {"破甲兵",150,90,1.05f,4.5f,38,1.8f,4,0},
 {"先锋坦克",330,420,1.f,4.8f,42,1.25f,6,0},
 {"长弓火炮",450,230,.7f,7.2f,78,3.f,8,0},
 {"采矿车",280,400,.85f,0,0,1,5,0},
 {"指挥中心",0,2200,0,0,0,1,1,-10},
 {"聚变电站",200,650,0,0,0,1,5,55},
 {"矿石精炼厂",350,900,0,0,0,1,7,-15},
 {"步兵营",220,700,0,0,0,1,5,-10},
 {"装甲工厂",500,1100,0,0,0,1,9,-30},
 {"哨戒炮塔",250,650,0,5.5f,32,1.2f,6,-15},
 {"曙光中继站",0,1100,0,0,0,1,1,-5},
 {"战地补给站",0,600,0,0,0,1,1,0}
};
struct Entity {Vec p{},goal{},step{};float hp=0,cool=0,progress=0,dir=0,flash=0,stuck=0;int16_t target=-1;uint8_t kind=0,team=0,cargo=0,kills=0;bool selected=false,moving=false;};
struct Shot {Vec p{},from{},dest{};float t=0,duration=0,damage=0;int16_t target=-1;uint8_t team=0,kind=0;bool alive=false;};
// Save v5 retains the v4 record sizes. Shot team bit 0 is allegiance;
// bits 1..7 are shooter slot + 1. Target bits 0..6 are the victim slot and
// bits 7..14 carry the shooter's spawn token (combat Entity::cargo).
// Legacy v4 shots have team 0/1, so their missing attribution stays unknown.
inline int shot_team(const Shot& s){return s.team&1;}
inline int shot_target(const Shot& s){return s.target<0?-1:(s.target&127);}
inline int shot_source(const Shot& s){return (s.team>>1)-1;}
inline unsigned veteran_rank(const Entity& a){return a.kills>=9?3:a.kills>=5?2:a.kills>=2?1:0;}
struct Blast {Vec p{};float age=0;uint8_t type=0;bool alive=false;};
struct Job {uint8_t kind=0;float left=0,total=0;};
struct World {
 std::array<Entity,MAX> e{};std::array<Shot,64> shots{};std::array<Blast,48> blasts{};
 std::array<uint8_t,N*N> terrain{},ore{},fog{},seen{},obstacles{};std::array<Job,6> jobs{};
 uint32_t rng=83711;int chapter=0,credits=1100,power_net=0,job_count=0,wave=0,killed=0,lost=0,earned=0;
 float time=0,next_wave=58,strike_cd=0,capture=0,autosave=0,notice_time=0,reveal_timer=0,radio_time=0;
 bool won=false,defeated=false,story_beat=false;char notice[128]{};unsigned sound=0;
 uint32_t random(){rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;return rng;}
 void say(const char* s,float duration=5){std::snprintf(notice,sizeof notice,"%s",s);notice_time=duration;}
 bool building(int k)const{return k>=hq;}
 int count(int k,int team=0)const{int n=0;for(auto& a:e)n+=a.hp>0&&a.team==team&&a.kind==k;return n;}
 int army(int team=0)const{int n=0;for(auto& a:e)n+=a.hp>0&&a.team==team&&a.kind<harvester;return n;}
 bool open(Vec p,int ignore=-1)const{if(p.x<1||p.y<1||p.x>=N-1||p.y>=N-1||terrain[cell(p)]==2)return false;for(int i=0;i<MAX;++i){auto& a=e[i];if(i!=ignore&&a.hp>0&&building(a.kind)&&((a.p.x-p.x)*(a.p.x-p.x)+(a.p.y-p.y)*(a.p.y-p.y))<(a.kind==turret?.4225f:1.44f))return false;}return true;}
 int add(int kind,int team,Vec p){for(int i=0;i<MAX;++i)if(e[i].hp<=0){e[i]={};auto& a=e[i];a.p=a.goal=a.step=p;a.hp=specs[kind].hp;a.kind=kind;a.team=team;if(kind<harvester||kind==turret)a.cargo=uint8_t(random()%255+1);a.dir=team?3.14f:0;return i;}return -1;}
 void init(int ch){*this=World{};chapter=std::clamp(ch,0,2);rng=83711+chapter*7919;credits=chapter?1350:1100;
  for(int y=0;y<N;++y)for(int x=0;x<N;++x){int c=y*N+x;terrain[c]=(random()%11==0)?1:0;if((x==0||y==0||x==N-1||y==N-1))terrain[c]=2;
   // A river cuts the highlands; broad stone crossings leave three tactical routes.
   if(chapter==1&&x>=13&&x<=15&&!(y>=5&&y<=8)&&!(y>=16&&y<=19)&&!(y>=23&&y<=25))terrain[c]=2;
   if((x>=3&&x<=6&&y>=13&&y<=16)||(x>=19&&x<=22&&y>=6&&y<=9)||(x>=7&&x<=10&&y>=21&&y<=23)){ore[c]=180+random()%76;terrain[c]=0;}}
  add(hq,0,{5.5f,8.5f});add(power,0,{3.5f,7.f});add(refinery,0,{4.f,11.f});add(barracks,0,{8.f,8.f});
  add(harvester,0,{5.f,12.5f});add(tank,0,{8.5f,11.f});add(tank,0,{9.5f,12.f});add(rifle,0,{8.f,13.f});add(rocket,0,{10.5f,11.5f});
  if(chapter){add(factory,0,{7.f,5.f});add(power,0,{3.f,4.f});add(artillery,0,{9.5f,9.5f});add(turret,0,{10.f,10.f});add(turret,0,{7.f,13.5f});}
  if(chapter==0){add(hq,1,{22.f,20.f});add(power,1,{24.f,17.f});add(turret,1,{19.f,19.f});add(barracks,1,{23.f,23.f});add(tank,1,{19.f,21.f});add(rifle,1,{18.f,20.f});}
  if(chapter==1){add(relay,2,{20.f,12.f});add(hq,1,{23.f,23.f});add(turret,1,{19.f,18.f});add(tank,1,{19.f,14.f});add(tank,1,{22.f,15.f});next_wave=50;}
  if(chapter==2){add(hq,1,{22.f,21.f});add(power,1,{19.f,18.f});add(power,1,{24.f,17.f});add(factory,1,{24.f,23.5f});add(turret,1,{18.f,21.f});add(turret,1,{22.f,17.f});add(tank,1,{18.f,18.f});add(tank,1,{21.f,18.f});add(artillery,1,{23.f,19.f});next_wave=65;}
  if(chapter==0){add(supply,2,{12,16.5f});add(supply,2,{18,8});}
  if(chapter==1){add(supply,2,{10,19});add(supply,2,{20,7});}
  if(chapter==2){add(supply,2,{14,13});add(supply,2,{24,10});}
  say("林岚：先稳住矿线，再组织装甲部队。",8);reveal();power_update();
 }
 void power_update(){obstacles.fill(0);for(int c=0;c<N*N;++c){if(terrain[c]==2){obstacles[c]=1;continue;}Vec p=center(c);for(auto& a:e)if(a.hp>0&&building(a.kind)&&((a.p.x-p.x)*(a.p.x-p.x)+(a.p.y-p.y)*(a.p.y-p.y))<(a.kind==turret?.4225f:1.44f)){obstacles[c]=1;break;}}power_net=0;for(auto& a:e)if(a.hp>0&&a.team==0&&a.progress<=0)power_net+=specs[a.kind].supply;}
 int ready_count(int k)const{int n=0;for(auto& a:e)n+=a.hp>0&&a.team==0&&a.kind==k&&a.progress<=0;return n;}
 bool unlocked(int k)const{if(k==hq||k==relay||k==supply)return false;if(k<=rocket)return ready_count(barracks)>0;if(k<=harvester)return ready_count(factory)>0;if(k==factory)return ready_count(barracks)>0&&ready_count(refinery)>0;if(k==turret)return ready_count(barracks)>0;return true;}
 const char* blocked(int k)const{if(!unlocked(k))return k<=rocket?"需要步兵营":k<=harvester?"需要装甲工厂":"需要兵营和精炼厂";if(credits<specs[k].cost)return "矿石不足，等待采矿车返航";if(job_count>=6)return "生产队列已满";int roster=army();for(int i=0;i<job_count;++i)roster+=jobs[i].kind<harvester;if(roster>=36&&k<harvester)return "部队已达 36 人上限";return nullptr;}
 bool queue(int k){if(auto reason=blocked(k)){say(reason);return false;}credits-=specs[k].cost;jobs[job_count++]={uint8_t(k),specs[k].build,specs[k].build};say("生产指令已确认");sound=1;return true;}
 void cancel_job(){if(job_count){credits+=specs[jobs[job_count-1].kind].cost;--job_count;say("已取消末项，资金退回");}}
 bool can_place(int,Vec p)const{if(!open(p)||!seen[cell(p)])return false;for(auto& a:e)if(a.hp>0&&a.team==0&&building(a.kind)&&dist(a.p,p)<6.f&&dist(a.p,p)>1.8f)return true;return false;}
 bool place(int k,Vec p){if(credits<specs[k].cost||!unlocked(k))return false;if(!can_place(k,p)){say("请建在己方基地附近的空地");return false;}int i=add(k,0,p);if(i<0){say("实体数量已达上限");return false;}credits-=specs[k].cost;e[i].progress=specs[k].build;say("工程队：开始施工。");sound=1;power_update();return true;}
 void select_all(){for(auto& a:e)a.selected=a.hp>0&&a.team==0&&a.kind<harvester;say("已选择全部作战部队");}
 int selected()const{int n=0;for(auto& a:e)n+=a.hp>0&&a.selected;return n;}
 void select_one(int id,bool append=false){for(int i=0;i<MAX;++i)if(!append)e[i].selected=false;if(id>=0&&id<MAX&&e[id].hp>0&&e[id].team==0)e[id].selected=true;}
 int hit(Vec p,float radius=1.f)const{int found=-1;float best=radius;for(int i=0;i<MAX;++i){auto& a=e[i];if(a.hp<=0||(!fog[cell(a.p)]&&a.team!=0))continue;float d=dist(p,a.p);if(d<best){best=d;found=i;}}return found;}
 void stop_selected(){int n=0;for(auto& a:e)if(a.hp>0&&a.selected&&a.team==0&&a.kind<harvester){a.goal=a.step=a.p;a.moving=false;a.target=-1;++n;}say(n?"编队就地警戒，自动还击":"请先选择作战部队");}
 void order(Vec p,int target=-1){int n=0,rallies=0;for(int i=0;i<MAX;++i){auto& a=e[i];if(a.hp<=0||!a.selected||a.team!=0)continue;
  if(a.kind==factory||a.kind==barracks){if(target<0&&open(p)){a.goal=p;a.moving=true;++rallies;}continue;}
  if(a.kind>=hq)continue;
  float ox=((n%4)-1.5f)*.65f,oy=(n/4)*.65f;a.goal={std::clamp(p.x+ox,1.2f,26.5f),std::clamp(p.y+oy,1.2f,26.5f)};if(!open(a.goal))a.goal=p;a.step=a.p;a.moving=true;a.target=target;++n;
 }
  if(n){say(target>=0?"集中火力，攻击目标！":"收到，编队前进。");sound=1;}else if(rallies){say("集结点已设置，新部队会自动前往");sound=1;}
 }
 [[gnu::noinline]] void reveal(){fog.fill(0);for(auto& a:e)if(a.hp>0&&a.team==0){int r=a.kind==hq?7:6;for(int y=std::max(0,int(a.p.y)-r);y<=std::min(N-1,int(a.p.y)+r);++y)for(int x=std::max(0,int(a.p.x)-r);x<=std::min(N-1,int(a.p.x)+r);++x)if(dist(a.p,{x+.5f,y+.5f})<r){fog[y*N+x]=1;seen[y*N+x]=1;}}}
 int nearest(Vec p,int kind,int team)const{float best=999;int id=-1;for(int i=0;i<MAX;++i){auto& a=e[i];if(a.hp>0&&a.progress<=0&&a.kind==kind&&a.team==team&&dist(p,a.p)<best){best=dist(p,a.p);id=i;}}return id;}
 int ore_near(Vec p)const{int id=-1;float best=999;for(int c=0;c<N*N;++c)if(ore[c]&&terrain[c]!=2){float d=dist(p,center(c));if(d<best){best=d;id=c;}}return id;}
 // Bounded breadth-first pathfinding. Units follow one cell at a time and
 // replan at cell boundaries; static water and building footprints are obstacles.
 [[gnu::noinline]] Vec path(Vec from,Vec to,int)const{
  int src=cell(from),dest=cell(to);if(src==dest)return to;
  std::array<int16_t,N*N> parent;parent.fill(-1);std::array<int16_t,N*N> q{};int head=0,tail=1;q[0]=src;parent[src]=src;int closest=src;float best=(from.x-to.x)*(from.x-to.x)+(from.y-to.y)*(from.y-to.y);
  while(head<tail){int c=q[head++];if(c==dest){closest=c;break;}int x=c%N,y=c/N;
   for(int d=0;d<4;++d){int xx=x+(d==0)-(d==1),yy=y+(d==2)-(d==3);if(xx<1||yy<1||xx>=N-1||yy>=N-1)continue;int next=yy*N+xx;if(parent[next]>=0||obstacles[next])continue;parent[next]=c;q[tail++]=next;float dd=(xx+.5f-to.x)*(xx+.5f-to.x)+(yy+.5f-to.y)*(yy+.5f-to.y);if(dd<best){best=dd;closest=next;}}
  }
  if(closest==src)return from;
  while(parent[closest]!=src&&parent[closest]!=closest)closest=parent[closest];
  return center(closest);
 }
 [[gnu::noinline]] void move(int id,float dt){auto& a=e[id];if(dist(a.p,a.goal)<.18f){a.moving=false;return;}if(dist(a.p,a.step)<.13f||!open(a.step,id))a.step=path(a.p,a.goal,id);float d=dist(a.p,a.step);if(d<.02f){a.stuck+=dt;if(a.stuck>2)a.moving=false;return;}a.stuck=0;float v=std::min(d,specs[a.kind].speed*dt);a.dir=std::atan2(a.step.y-a.p.y,a.step.x-a.p.x);a.p.x+=(a.step.x-a.p.x)/d*v;a.p.y+=(a.step.y-a.p.y)/d*v;
  // Soft separation makes formations readable without deadlocking a crossing.
  for(int j=0;j<id;++j)if(e[j].hp>0&&e[j].kind<hq){float dd=dist(a.p,e[j].p);if(dd>.01f&&dd<.38f){Vec n={a.p.x+(a.p.x-e[j].p.x)/dd*dt*.25f,a.p.y+(a.p.y-e[j].p.y)/dd*dt*.25f};if(open(n,id))a.p=n;}}
 }
 void blast(Vec p,int type=0){for(auto& b:blasts)if(!b.alive){b={p,0,uint8_t(type),true};return;}}
 void damage(int id,float value,int source=-1){
  if(id<0||id>=MAX||e[id].hp<=0)return;auto& a=e[id];a.hp-=value;a.flash=.14f;
  if(a.hp>0)return;
  a.hp=0;blast(a.p,building(a.kind)?2:1);sound=3;
  int c=cell(a.p);if(terrain[c]!=2&&!ore[c])terrain[c]=building(a.kind)?5:4;
  if(a.team==1){++killed;credits+=a.kind>=hq?100:25;}else if(a.team==0)++lost;
  if(source>=0&&source<MAX&&e[source].hp>0&&e[source].team!=a.team&&e[source].team!=2&&a.team!=2){
   auto& killer=e[source];unsigned before=veteran_rank(killer);killer.kills=std::min(255,int(killer.kills)+1);
   if(veteran_rank(killer)>before){if(killer.hp<specs[killer.kind].hp)killer.hp=std::min(float(specs[killer.kind].hp),killer.hp+specs[killer.kind].hp*.2f);blast(killer.p,4);if(killer.team==0){say("部队晋升！火力增强，战地修复已完成");sound=4;}}
  }
  power_update();
 }
 void fire(int id,int target){auto& a=e[id];bool admitted=false;
  for(auto& s:shots)if(!s.alive){s={};s.from=a.p;s.p=a.p;s.dest=e[target].p;s.target=int16_t(target|(unsigned(a.cargo)<<7));s.team=uint8_t((id+1)*2+a.team);s.kind=a.kind;s.damage=specs[a.kind].damage*(1+veteran_rank(a)*.12f);s.duration=a.kind==artillery?.85f:a.kind==rocket?.5f:.22f;s.alive=true;admitted=true;break;}
  if(!admitted)return;a.cool=specs[a.kind].reload;a.flash=.08f;a.dir=std::atan2(e[target].p.y-a.p.y,e[target].p.x-a.p.x);blast({a.p.x+std::cos(a.dir)*.8f,a.p.y+std::sin(a.dir)*.8f},5);if(sound<2)sound=2;
 }
 void supply_tick(Entity& station,float dt){
  bool friendly=false,contested=false;for(auto& a:e)if(a.hp>0&&a.kind<harvester&&dist(a.p,station.p)<2.7f){friendly|=a.team==0;contested|=a.team==1;}
  if(!friendly){station.progress=std::max(0.f,station.progress-dt*.5f);return;}
  if(contested)return;station.progress+=dt;if(station.progress<5)return;
  credits+=280;for(auto& a:e)if(a.hp>0&&a.team==0&&a.kind<harvester&&dist(a.p,station.p)<4){if(a.hp<specs[a.kind].hp)a.hp=std::min(float(specs[a.kind].hp),a.hp+specs[a.kind].hp*.3f);blast(a.p,3);}
  station.hp=0;terrain[cell(station.p)]=3;blast(station.p,6);power_update();say("补给接收！矿石 +280，附近部队已维修",7);sound=5;
 }
 bool strike(Vec p){if(strike_cd>0){say("支援还在冷却中");return false;}if(credits<180){say("空袭需要 180 矿石");return false;}if(!fog[cell(p)]){say("空袭需要可见目标");return false;}credits-=180;strike_cd=45;for(int i=0;i<MAX;++i)if(e[i].hp>0&&e[i].team==1&&dist(e[i].p,p)<3.8f)damage(i,260);for(int i=0;i<7;++i)blast({p.x+(int(random()%400)-200)*.01f,p.y+(int(random()%400)-200)*.01f},2);say("空中支援：雷鸣打击已抵达！");sound=3;return true;}
 void repair(){for(auto& a:e)if(a.selected&&a.hp>0&&a.team==0){int cost=int((specs[a.kind].hp-a.hp)*.2f);if(cost<=0)continue;if(credits<cost){say("修复资金不足");return;}credits-=cost;a.hp=specs[a.kind].hp;blast(a.p,3);}say("维修完成");}
 void wave_spawn(){++wave;next_wave=time+std::max(24.f,55.f-wave*2.f-chapter*4.f);Vec spawn=chapter==1?Vec{23,24}:Vec{24,22};int units=std::min(7,2+wave/2+(chapter==2?1:chapter));for(int j=0;j<units;++j){int k=j%3==0?tank:j%3==1?rifle:rocket;if(chapter==2&&wave>=3&&j==units-1)k=artillery;int id=add(k,1,{spawn.x+(j%3)*.6f,spawn.y+(j/3)*.6f});if(id>=0){e[id].goal=chapter==1&&count(relay,0)?Vec{20,12}:Vec{7,10};e[id].moving=true;e[id].step=e[id].p;}}say("侦察：敌方增援正向基地推进！",6);}
 [[gnu::noinline]] void tick(float dt){if(won||defeated)return;dt=std::clamp(dt,0.f,.1f);time+=dt;autosave+=dt;notice_time=std::max(0.f,notice_time-dt);strike_cd=std::max(0.f,strike_cd-dt);reveal_timer-=dt;
  if(reveal_timer<=0){reveal();reveal_timer=.5f;}if(time>=next_wave&&count(hq,1))wave_spawn();
  if(job_count){jobs[0].left-=dt*(power_net<0?.45f:1.f);if(jobs[0].left<=0){int k=jobs[0].kind;int home=nearest({6,8},k<=rocket?barracks:factory,0);if(home<0){say("生产设施已损毁，资金已退回");credits+=specs[k].cost;}else {Vec pos={e[home].p.x+1.6f,e[home].p.y+1.6f};int id=add(k,0,pos);if(id>=0){blast(pos,3);if(e[home].moving&&open(e[home].goal)){e[id].goal=e[home].goal;e[id].step=pos;e[id].moving=true;}say("新部队已就绪");sound=1;}else credits+=specs[k].cost;}for(int j=1;j<job_count;++j)jobs[j-1]=jobs[j];--job_count;}}
  for(int i=0;i<MAX;++i){auto& a=e[i];if(a.hp<=0)continue;a.flash=std::max(0.f,a.flash-dt);a.cool=std::max(0.f,a.cool-dt);
   if(a.kind==supply){supply_tick(a,dt);continue;}
   if(a.progress>0){a.progress-=dt*(power_net<0?.5f:1.f);if(a.progress<=0){a.progress=0;say("工程完成");if(a.kind==refinery){add(harvester,0,{a.p.x+1.5f,a.p.y+1.5f});}power_update();}continue;}
   if(a.kind==harvester&&a.team==0){if(a.cargo>=85){int r=nearest(a.p,refinery,0);if(r>=0){a.goal={e[r].p.x+1.4f,e[r].p.y+1.4f};if(dist(a.p,e[r].p)<2.3f){credits+=a.cargo;earned+=a.cargo;a.cargo=0;blast(a.p,3);}}}
    else {int c=ore_near(a.p);if(c>=0){a.goal=center(c);if(dist(a.p,a.goal)<.65f&&a.cool<=0){int amt=std::min({10,int(ore[c]),85-int(a.cargo)});ore[c]-=amt;a.cargo+=amt;a.cool=.55f;}}else if(a.cargo){a.cargo=85;}else if(time>30&&int(time)%20==0)say("矿脉已枯竭，请拓展新的矿区");}
    a.moving=dist(a.p,a.goal)>.2f;if(a.moving)move(i,dt);continue;}
   if(a.kind==relay||a.kind==hq||a.kind==power||a.kind==refinery||a.kind==barracks||a.kind==factory)continue;
   int target=-1;float best=specs[a.kind].range;
   if(a.target>=0&&a.target<MAX&&e[a.target].hp>0&&e[a.target].team!=a.team&&e[a.target].team!=2){if(dist(a.p,e[a.target].p)<best+(building(e[a.target].kind)?1:0))target=a.target;else if(a.kind<hq){a.goal=e[a.target].p;a.moving=true;}}
   else a.target=-1;
   if(target<0)for(int j=0;j<MAX;++j){auto& b=e[j];if(b.hp<=0||b.team==a.team||b.team==2||(!fog[cell(b.p)]&&a.team==0))continue;float d=dist(a.p,b.p)-(building(b.kind)?.6f:0);if(d<best){target=j;best=d;}}
   bool powered=a.kind!=turret||a.team==1||power_net>=0;
   if(target>=0&&powered){if(a.cool<=0)fire(i,target);}else if(a.moving&&a.kind<hq)move(i,dt);
  }
  for(auto& s:shots)if(s.alive){s.t+=dt;float f=std::min(1.f,s.t/s.duration);s.p={s.from.x+(s.dest.x-s.from.x)*f,s.from.y+(s.dest.y-s.from.y)*f};if(f>=1){
   s.alive=false;float value=s.damage;int victim_id=shot_target(s),source=shot_source(s);
   if(source<0||source>=MAX||e[source].hp<=0||e[source].team!=shot_team(s)||e[source].kind!=s.kind||e[source].cargo!=(unsigned(s.target)>>7))source=-1;
   if(victim_id>=0&&victim_id<MAX){auto& victim=e[victim_id];if(dist(victim.p,s.dest)<1.8f){if(s.kind==rifle&&victim.kind>=tank)value*=.32f;if(s.kind==rocket&&victim.kind==rifle)value*=.55f;damage(victim_id,value,source);}}
   blast(s.dest,0);if(s.kind==artillery)for(int j=0;j<MAX;++j)if(j!=victim_id&&e[j].hp>0&&e[j].team!=shot_team(s)&&e[j].team!=2&&dist(e[j].p,s.dest)<1.7f)damage(j,value*.5f,source);
  }}
  for(auto& b:blasts)if(b.alive){b.age+=dt;float life=b.type==2?2.2f:b.type==1?1.1f:b.type==3?.7f:b.type==4?1.f:b.type==5?.16f:b.type==6?1.2f:.65f;if(b.age>life)b.alive=false;}
  if(chapter==1){int r=nearest({20,12},relay,2);if(r>=0){bool near=false;for(auto& a:e)if(a.hp>0&&a.team==0&&a.kind<harvester&&dist(a.p,e[r].p)<3)near=true;if(near){capture+=dt;if(capture>=8){e[r].team=0;story_beat=true;say("苏弥：中继站已上线。坚持住，平民正在撤离！",8);}}}if(count(relay,0)){radio_time+=dt;if(radio_time>=90)won=true;}if(!count(relay,0)&&!count(relay,2))defeated=true;}
  else if(!count(hq,1)&&(chapter!=2||!count(power,1)))won=true;
  if(!count(hq,0))defeated=true;
  if(chapter==0&&!story_beat&&time>50){story_beat=true;say("苏弥：灰塔在用民用电网供能。我们必须查清真相。",8);}
  if(chapter==2&&!count(power,1)&&!story_beat){story_beat=true;for(auto& a:e)if(a.team==1&&a.kind==turret)a.cool=9999;say("林岚：敌方电网瘫痪！全军向灰塔推进！",8);}
 }
};
}
