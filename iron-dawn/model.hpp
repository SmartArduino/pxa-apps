// SPDX-License-Identifier: MIT
#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <new>
namespace iron {
constexpr int N=48,MAX=100,CHAPTERS=6;
struct Vec {float x=0,y=0;};
inline float dist(Vec a,Vec b){return std::hypot(a.x-b.x,a.y-b.y);}
inline int cell(Vec p){return std::clamp(int(p.y),0,N-1)*N+std::clamp(int(p.x),0,N-1);}
inline Vec center(int c){return {float(c%N)+.5f,float(c/N)+.5f};}
enum Terrain:uint8_t {ground=0,woodland=1,water=2,cleared=3,crater=4,ruins=5,road=6,bridge=7,ridge=8};
enum Kind:uint8_t {rifle,rocket,tank,artillery,harvester,hq,power,refinery,barracks,factory,turret,relay,supply};
struct Spec {const char* name;int cost,hp;float speed,range,damage,reload,build;int supply;};
inline constexpr Spec specs[]={
 {"游骑兵",90,100,1.15f,3.5f,12,.65f,3,0},
 {"破甲兵",150,90,1.f,4.5f,38,1.8f,4,0},
 {"先锋坦克",330,420,1.95f,4.8f,42,1.25f,6,0},
 {"长弓火炮",450,230,1.5f,7.2f,78,3.f,8,0},
 {"采矿车",280,400,1.75f,0,0,1,5,0},
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
struct Expansion {uint32_t mode=0,seed=0,cleared=0;uint8_t damage=0,armor=0,research=0,map_rules=0;float research_left=0;};
struct Mission {uint32_t events=0,read_events=0;uint8_t stage=0,delivered=0;std::array<uint8_t,3> waypoint{};std::array<int16_t,3> convoy{{-1,-1,-1}};float hold=0;};
struct World {
 std::array<Entity,MAX> e{};std::array<Shot,64> shots{};std::array<Blast,48> blasts{};
 std::array<uint8_t,N*N> terrain{},ore{},fog{},seen{},obstacles{};std::array<Job,6> jobs{};
 uint32_t rng=83711;int chapter=0,credits=1100,power_net=0,job_count=0,wave=0,killed=0,lost=0,earned=0;
 float time=0,next_wave=58,strike_cd=0,capture=0,autosave=0,notice_time=0,reveal_timer=0,radio_time=0;
 bool won=false,defeated=false,story_beat=false;char notice[128]{};unsigned sound=0;Expansion extra{};
 int side=28;Mission mission{};unsigned route_credits=6;
 void emit_sound(unsigned cue){if(sound!=12&&(sound!=13||cue==12))sound=cue;}
 uint32_t random(){rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;return rng;}
 void say(const char* s,float duration=5){std::snprintf(notice,sizeof notice,"%s",s);notice_time=duration;}
 bool building(int k)const{return k>=hq;}
 int count(int k,int team=0)const{int n=0;for(auto& a:e)n+=a.hp>0&&a.team==team&&a.kind==k;return n;}
 int army(int team=0)const{int n=0;for(auto& a:e)n+=a.hp>0&&a.team==team&&a.kind<harvester;return n;}
 bool open(Vec p,int ignore=-1)const{if(p.x<1||p.y<1||p.x>=side-1||p.y>=side-1||(terrain[cell(p)]==water||terrain[cell(p)]==ridge))return false;for(int i=0;i<MAX;++i){auto& a=e[i];if(i!=ignore&&a.hp>0&&building(a.kind)&&((a.p.x-p.x)*(a.p.x-p.x)+(a.p.y-p.y)*(a.p.y-p.y))<(a.kind==turret?.4225f:1.44f))return false;}return true;}
 int add(int kind,int team,Vec p){for(int i=0;i<MAX;++i)if(e[i].hp<=0){e[i]={};auto& a=e[i];a.p=a.goal=a.step=p;a.hp=specs[kind].hp;a.kind=kind;a.team=team;if(kind<harvester||kind==turret)a.cargo=uint8_t(random()%255+1);a.dir=team?3.14f:0;return i;}return -1;}
 void init(int ch){new(this) World{};chapter=std::clamp(ch,0,CHAPTERS-1);if(chapter>=3){init_frontier();return;}terrain.fill(water);extra.map_rules=1;rng=83711+chapter*7919;credits=chapter?1350:1100;
  for(int y=0;y<side;++y)for(int x=0;x<side;++x){int c=y*N+x;terrain[c]=(random()%11==0)?1:0;if((x==0||y==0||x==side-1||y==side-1))terrain[c]=2;
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
  build_landscape();say("林岚：先稳住矿线，再组织装甲部队。",8);reveal();power_update();
 }
 // Independent defense games preserve the three story chapters and their unlocks.
 void init_defense(uint32_t seed){init(0);extra.mode=1;extra.seed=seed?seed:83711;rng=extra.seed;credits=1500;next_wave=35;
  for(auto& a:e)if(a.team!=0)a.hp=0;
  for(int y=1;y<side-1;++y)for(int x=1;x<side-1;++x){int c=y*N+x;terrain[c]=ore[c]?0:(random()%13==0?1:0);}
  for(int y=1;y<side-1;++y)if(!defense_crossing(y))for(int x=17;x<=18;++x){terrain[y*N+x]=2;ore[y*N+x]=0;}
  add(factory,0,{7,5});add(power,0,{3,4});add(turret,0,{10,9});
  add(supply,2,{13,8});add(supply,2,{9,18});add(supply,2,{19,16});
  for(int y=18;y<21;++y)for(int x=13;x<16;++x)ore[y*N+x]=200+random()%55;
  build_landscape();say("苏弥：守住前哨。每次击退敌军，补给队就能靠近。",8);power_update();reveal();
 }
 // Terrain is stored in the existing byte grid; published save prefixes stay intact.
 void build_landscape(){
  if(extra.mode||chapter==1)for(int y=2;y<side-2;++y){auto crossing=[&](int row){return extra.mode?defense_crossing(row):((row>=5&&row<=8)||(row>=16&&row<=19)||(row>=23&&row<=25));};if(crossing(y-1)||crossing(y)||crossing(y+1))continue;for(int side=0;side<2;++side){int x=extra.mode?(side?19:16):(side?16:12),c=y*N+x;unsigned hash=x*97+y*53+extra.seed;if(hash%4!=0||ore[c]||terrain[c]!=ground)continue;bool occupied=false;for(auto& a:e)if(a.hp>0&&dist(a.p,center(c))<2.5f){occupied=true;break;}if(!occupied)terrain[c]=water;}}
  auto rocks=[&](int left,int top,int width,int height){for(int y=top;y<top+height;++y)for(int x=left;x<left+width;++x){int c=y*N+x;if(terrain[c]==water||ore[c])continue;bool occupied=false;for(auto& a:e)if(a.hp>0&&dist(a.p,center(c))<2.3f){occupied=true;break;}if(!occupied)terrain[c]=ridge;}};
  if(extra.mode){rocks(13,3,2,3);rocks(12,12,2,3);rocks(20,18,2,3);}else if(chapter==0){rocks(14,4,2,3);rocks(14,13,2,3);rocks(12,20,2,3);}else if(chapter==1){rocks(10,3,2,2);rocks(18,21,2,3);rocks(23,10,2,3);}else{rocks(14,11,2,3);rocks(13,19,2,3);rocks(20,10,2,3);}
  auto paving=[&](int x,int y){if(x<=0||y<=0||x>=side-1||y>=side-1)return;int c=y*N+x;if(!ore[c]&&terrain[c]!=water)terrain[c]=road;};
  for(int x=2;x<=25;++x)for(int y=9;y<=10;++y)paving(x,y);for(int y=3;y<=25;++y)for(int x=10;x<=11;++x)paving(x,y);
  for(int x=8;x<=24;++x)for(int y=17;y<=18;++y)paving(x,y);for(int y=9;y<=23;++y)for(int x=20;x<=21;++x)paving(x,y);
  // Soft woodland patches leave roads, mine cells and building approaches clear.
  for(Vec patch:{Vec{9,16},Vec{16,6},Vec{23,13},Vec{8,21}})for(int y=int(patch.y)-2;y<=int(patch.y)+2;++y)for(int x=int(patch.x)-2;x<=int(patch.x)+2;++x){int c=y*N+x;Vec p=center(c);if(terrain[c]!=ground||ore[c]||dist(p,patch)>2.2f)continue;bool occupied=false;for(auto& a:e)if(a.hp>0&&building(a.kind)&&dist(a.p,p)<2.4f){occupied=true;break;}if(!occupied)terrain[c]=woodland;}
  if(extra.mode){for(int y=1;y<side-1;++y)if(defense_crossing(y))for(int x=17;x<=18;++x)terrain[y*N+x]=bridge,ore[y*N+x]=0;}
  else if(chapter==1){for(int y=1;y<side-1;++y)if((y>=5&&y<=8)||(y>=16&&y<=19)||(y>=23&&y<=25))for(int x=13;x<=15;++x)terrain[y*N+x]=bridge,ore[y*N+x]=0;}
  // Forward deposits reward opening a second mine without replacing the safe starter ore.
  for(int y=20;y<=21;++y)for(int x=16;x<=17;++x){int c=y*N+x;if(terrain[c]==ground||terrain[c]==woodland)ore[c]=160+unsigned(x*19+y*11+chapter*23)%65;}
 }
 bool paved(Vec p)const{return terrain[cell(p)]==road||terrain[cell(p)]==bridge;}
 bool in_cover(const Entity& a)const{return extra.map_rules&&a.kind<=rocket&&(terrain[cell(a.p)]==woodland||terrain[cell(a.p)]==crater||terrain[cell(a.p)]==ruins);}
 float move_speed(const Entity& a)const{float speed=specs[a.kind].speed;bool vehicle=a.kind>=tank&&a.kind<=harvester;if(a.kind==harvester&&a.cargo)speed=1.42f;if(paved(a.p))speed*=vehicle?1.2f:1.05f;else if(terrain[cell(a.p)]==woodland)speed*=vehicle?.82f:.95f;else if(terrain[cell(a.p)]==crater||terrain[cell(a.p)]==ruins)speed*=vehicle?.78f:.9f;return speed;}
 Vec wave_entry(int next)const{if(chapter>=3&&!extra.mode){return next%2?Vec{float(side-5),6}:Vec{float(side-5),float(side-6)};}if(extra.mode){constexpr Vec edges[]={{23,5},{24,22},{5,24}};return edges[(extra.seed+unsigned(next))%3];}return chapter==1?Vec{23,24}:Vec{24,22};}
 const char* wave_direction(int next)const{auto p=wave_entry(next);return p.x<10?"南路":p.y<10?"东北":"东南";}
 Vec raid_goal(int next,int unit)const{if(chapter==4&&!extra.mode&&unit%2){for(int id:mission.convoy)if(id>=0&&e[id].hp>0)return e[id].p;}if(chapter==1&&!extra.mode&&count(relay,0))return {20,12};if(extra.map_rules&&next>=2&&unit%3==1)return next%2?Vec{5,13}:Vec{7,5};return {7,10};}
 bool defense_crossing(int y)const{int first=6+int(extra.seed%2)*2,second=15+int(extra.seed%3);return (y>=first&&y<=first+3)||(y>=second&&y<=second+3)||(y>=23&&y<=25);}
 bool defense_bridge(int x,int y)const{if(x<17||x>18)return false;int first=6+int(extra.seed%2)*2,second=15+int(extra.seed%3);return y==first||y==first+3||y==second||y==second+3||y==23||y==25;}
 unsigned tech_level(int kind)const{return kind==1?extra.damage:extra.armor;}
 int research_cost(int kind)const{return tech_level(kind)?400:220;}
 float research_duration(int kind)const{return tech_level(kind)?24.f:16.f;}
 const char* research_blocked(int kind)const{if(kind<1||kind>2)return "未知研发项目";if(extra.research)return "工程师正在研发";if(tech_level(kind)>=2)return "科技已达最高等级";if(!ready_count(factory))return "需要建成装甲工厂";if(credits<research_cost(kind))return "研发矿石不足";return nullptr;}
 bool research_start(int kind){if(auto why=research_blocked(kind)){say(why);return false;}credits-=research_cost(kind);extra.research=kind;extra.research_left=research_duration(kind);say("研发已启动，战局继续进行");emit_sound(1);return true;}
 void research_cancel(){if(!extra.research)return;credits+=research_cost(extra.research);extra.research=0;extra.research_left=0;say("研发已取消，矿石已退回");}
 void research_tick(float dt){if(!extra.research||!ready_count(factory))return;extra.research_left=std::max(0.f,extra.research_left-dt*(power_net<0?.45f:1.f));if(extra.research_left>0)return;
  bool ammo=extra.research==1;if(ammo)++extra.damage;else ++extra.armor;extra.research=0;
  for(auto& a:e)if(a.hp>0&&a.team==0&&a.kind==hq)blast(a.p,4);
  say(ammo?"弹药改良完成：作战火力增强":"复合装甲完成：受到伤害降低",7);emit_sound(4);
 }
 void defense_clear(){extra.cleared=wave;int reward=150+25*wave;credits=std::min(10000000,credits+reward);next_wave=time+18;
  int base=nearest({6,9},hq,0);if(base>=0)for(auto& a:e)if(a.hp>0&&a.team==0&&a.kind<hq&&dist(a.p,e[base].p)<8){if(a.hp<specs[a.kind].hp)a.hp=std::min(float(specs[a.kind].hp),a.hp+specs[a.kind].hp*.12f);blast(a.p,3);}
  if(wave%3==0){Vec station=wave%2?Vec{15,16}:Vec{18,8};if(open(station))add(supply,2,station);}
  char text[120];std::snprintf(text,sizeof text,"第 %d 波已击退！补给 +%d，前哨维修完成",wave,reward);say(text,8);emit_sound(5);
 }
 void power_update(){obstacles.fill(0);for(int c=0;c<N*N;++c){if(terrain[c]==water||terrain[c]==ridge){obstacles[c]=1;continue;}Vec p=center(c);for(auto& a:e)if(a.hp>0&&building(a.kind)&&((a.p.x-p.x)*(a.p.x-p.x)+(a.p.y-p.y)*(a.p.y-p.y))<(a.kind==turret?.4225f:1.44f)){obstacles[c]=1;break;}}power_net=0;for(auto& a:e)if(a.hp>0&&a.team==0&&a.progress<=0)power_net+=specs[a.kind].supply;}
 int ready_count(int k)const{int n=0;for(auto& a:e)n+=a.hp>0&&a.team==0&&a.kind==k&&a.progress<=0;return n;}
 bool unlocked(int k)const{if(k==hq||k==relay||k==supply)return false;if(k<=rocket)return ready_count(barracks)>0;if(k<=harvester)return ready_count(factory)>0;if(k==factory)return ready_count(barracks)>0&&ready_count(refinery)>0;if(k==turret)return ready_count(barracks)>0;return true;}
 const char* blocked(int k)const{if(!unlocked(k))return k<=rocket?"需要步兵营":k<=harvester?"需要装甲工厂":"需要兵营和精炼厂";if(credits<specs[k].cost)return "矿石不足，等待采矿车返航";if(job_count>=6)return "生产队列已满";int roster=army();for(int i=0;i<job_count;++i)roster+=jobs[i].kind<harvester;if(roster>=36&&k<harvester)return "部队已达 36 人上限";return nullptr;}
 bool queue(int k){if(auto reason=blocked(k)){say(reason);return false;}credits-=specs[k].cost;jobs[job_count++]={uint8_t(k),specs[k].build,specs[k].build};say("生产指令已确认");emit_sound(1);return true;}
 void cancel_job(){if(job_count){credits+=specs[jobs[job_count-1].kind].cost;--job_count;say("已取消末项，资金退回");}}
 bool can_place(int,Vec p)const{if(!open(p)||!seen[cell(p)]||terrain[cell(p)]==bridge)return false;for(auto& a:e)if(a.hp>0&&a.team==0&&building(a.kind)&&dist(a.p,p)<6.f&&dist(a.p,p)>1.8f)return true;return false;}
 bool place(int k,Vec p){if(credits<specs[k].cost||!unlocked(k))return false;if(!can_place(k,p)){say("请建在己方基地附近的空地");return false;}int i=add(k,0,p);if(i<0){say("实体数量已达上限");return false;}credits-=specs[k].cost;e[i].progress=specs[k].build;say("工程队：开始施工。");emit_sound(10);power_update();return true;}
 void select_all(){for(auto& a:e)a.selected=a.hp>0&&a.team==0&&a.kind<harvester;say("已选择全部作战部队");}
 int selected()const{int n=0;for(auto& a:e)n+=a.hp>0&&a.selected;return n;}
 void select_one(int id,bool append=false){for(int i=0;i<MAX;++i)if(!append)e[i].selected=false;if(id>=0&&id<MAX&&e[id].hp>0&&e[id].team==0)e[id].selected=true;}
 int hit(Vec p,float radius=1.f)const{int found=-1;float best=radius;for(int i=0;i<MAX;++i){auto& a=e[i];if(a.hp<=0||(!fog[cell(a.p)]&&a.team!=0))continue;float d=dist(p,a.p);if(d<best){best=d;found=i;}}return found;}
 void stop_selected(){int n=0;for(auto& a:e)if(a.hp>0&&a.selected&&a.team==0&&a.kind<harvester){a.goal=a.step=a.p;a.moving=false;a.target=-1;++n;}say(n?"编队就地警戒，自动还击":"请先选择作战部队");}
 void order(Vec p,int target=-1){int n=0,rallies=0;for(int i=0;i<MAX;++i){auto& a=e[i];if(a.hp<=0||!a.selected||a.team!=0)continue;
  if(a.kind==factory||a.kind==barracks){if(target<0&&open(p)){a.goal=p;a.moving=true;++rallies;}continue;}
  if(a.kind>=hq)continue;
  float ox=((n%4)-1.5f)*.65f,oy=(n/4)*.65f;a.goal={std::clamp(p.x+ox,1.2f,side-1.5f),std::clamp(p.y+oy,1.2f,side-1.5f)};if(!open(a.goal))a.goal=p;a.step=a.p;a.moving=true;a.target=target;++n;
 }
  if(n){say(target>=0?"集中火力，攻击目标！":"收到，编队前进。");emit_sound(1);}else if(rallies){say("集结点已设置，新部队会自动前往");emit_sound(1);}
 }
 [[gnu::noinline]] void reveal(){fog.fill(0);for(auto& a:e)if(a.hp>0&&a.team==0){int r=a.kind==hq?7:6;for(int y=std::max(0,int(a.p.y)-r);y<=std::min(side-1,int(a.p.y)+r);++y)for(int x=std::max(0,int(a.p.x)-r);x<=std::min(side-1,int(a.p.x)+r);++x)if(dist(a.p,{x+.5f,y+.5f})<r){fog[y*N+x]=1;seen[y*N+x]=1;}}}
 int nearest(Vec p,int kind,int team)const{float best=999;int id=-1;for(int i=0;i<MAX;++i){auto& a=e[i];if(a.hp>0&&a.progress<=0&&a.kind==kind&&a.team==team&&dist(p,a.p)<best){best=dist(p,a.p);id=i;}}return id;}
 // One bounded flood fill finds a reachable ore cell, rather than sending trucks
 // toward a nearer deposit behind a river or a closed building wall.
 int ore_near(Vec p)const{int src=cell(p);std::array<uint8_t,N*N> seen_cells{};std::array<int16_t,N*N> queue{};int head=0,tail=1;queue[0]=src;seen_cells[src]=1;
  while(head<tail){int c=queue[head++];if(ore[c]&&!obstacles[c])return c;int x=c%N,y=c/N;for(int d=0;d<4;++d){int xx=x+(d==0)-(d==1),yy=y+(d==2)-(d==3);if(xx<1||yy<1||xx>=side-1||yy>=side-1)continue;int n=yy*N+xx;if(seen_cells[n]||obstacles[n])continue;seen_cells[n]=1;queue[tail++]=n;}}return -1;
 }
 void harvest(int id,float dt){auto& a=e[id];int deposit=-1;if(a.cargo>=85)a.target=-2; // -2 is the truck's return-to-refinery state.
  if(a.target!=-2){int c=cell(a.goal);if(ore[c]&&!obstacles[c]&&a.stuck<1.5f)deposit=c;else if(a.cool<=0||a.moving)deposit=ore_near(a.p);else {a.moving=false;return;}if(deposit<0&&a.cargo)a.target=-2;}
  if(a.target==-2){int refinery_id=nearest(a.p,refinery,0);if(refinery_id<0){a.moving=false;return;}auto home=e[refinery_id].p;a.goal={home.x+1.4f,home.y+1.4f};if(dist(a.p,home)<2.3f){credits+=a.cargo;earned+=a.cargo;a.cargo=0;a.target=-1;a.goal=a.p;blast(a.p,3);}}
  else if(deposit>=0){a.goal=center(deposit);if(dist(a.p,a.goal)<.65f&&a.cool<=0){int amount=std::min({10,int(ore[deposit]),85-int(a.cargo)});ore[deposit]-=amount;a.cargo+=amount;a.cool=.55f;}}
  else {a.goal=a.p;a.moving=false;a.cool=2;if(time>30&&notice_time<=0)say("矿脉已枯竭，请拓展新的矿区");return;}
  a.moving=dist(a.p,a.goal)>.2f;if(a.moving)move(id,dt);
 }
 // Bounded breadth-first pathfinding. Units follow one cell at a time and
 // replan at cell boundaries; static water and building footprints are obstacles.
 [[gnu::noinline]] Vec path(Vec from,Vec to,int)const{
  int src=cell(from),dest=cell(to);if(src==dest)return to;
  std::array<int16_t,N*N> parent;parent.fill(-1);std::array<int16_t,N*N> q{};int head=0,tail=1;q[0]=src;parent[src]=src;int closest=src;float best=(from.x-to.x)*(from.x-to.x)+(from.y-to.y)*(from.y-to.y);
  while(head<tail){int c=q[head++];if(c==dest){closest=c;break;}int x=c%N,y=c/N;
   for(int d=0;d<4;++d){int xx=x+(d==0)-(d==1),yy=y+(d==2)-(d==3);if(xx<1||yy<1||xx>=side-1||yy>=side-1)continue;int next=yy*N+xx;if(parent[next]>=0||obstacles[next])continue;parent[next]=c;q[tail++]=next;float dd=(xx+.5f-to.x)*(xx+.5f-to.x)+(yy+.5f-to.y)*(yy+.5f-to.y);if(dd<best){best=dd;closest=next;}}
  }
  if(closest==src)return from;
  while(parent[closest]!=src&&parent[closest]!=closest)closest=parent[closest];
  return center(closest);
 }
 [[gnu::noinline]] void move(int id,float dt){auto& a=e[id];if(dist(a.p,a.goal)<.18f){a.moving=false;return;}if(dist(a.p,a.step)<.13f||!open(a.step,id)){if(!route_credits)return;--route_credits;a.step=path(a.p,a.goal,id);}float d=dist(a.p,a.step);if(d<.02f){a.stuck+=dt;if(a.stuck>2)a.moving=false;return;}a.stuck=0;float heading=std::atan2(a.step.y-a.p.y,a.step.x-a.p.x),alignment=1;if(a.kind>=tank&&a.kind<=harvester){float difference=std::remainder(heading-a.dir,6.2831853f),turn=(a.kind==artillery?2.6f:a.kind==harvester?4.2f:3.5f)*dt;a.dir=std::remainder(a.dir+std::clamp(difference,-turn,turn),6.2831853f);alignment=std::max(0.f,std::cos(std::remainder(heading-a.dir,6.2831853f)));}else a.dir=heading;float v=std::min(d,move_speed(a)*alignment*dt);a.p.x+=(a.step.x-a.p.x)/d*v;a.p.y+=(a.step.y-a.p.y)/d*v;
  // Soft separation makes formations readable without deadlocking a crossing.
  for(int j=0;j<id;++j)if(e[j].hp>0&&e[j].kind<hq){float dd=dist(a.p,e[j].p);if(dd>.01f&&dd<.38f){Vec n={a.p.x+(a.p.x-e[j].p.x)/dd*dt*.25f,a.p.y+(a.p.y-e[j].p.y)/dd*dt*.25f};if(open(n,id))a.p=n;}}
 }
 void blast(Vec p,int type=0){for(auto& b:blasts)if(!b.alive){b={p,0,uint8_t(type),true};return;}}
 void damage(int id,float value,int source=-1,int weapon=-1){
  if(id<0||id>=MAX||e[id].hp<=0)return;auto& a=e[id];if(!extra.mode&&chapter==5&&a.team==1&&a.kind==hq&&mission.stage<2)return;if(a.team==0)value*=1-extra.armor*.10f;int attack_kind=weapon>=0?weapon:source>=0&&source<MAX?int(e[source].kind):-1;if(in_cover(a)&&attack_kind>=0&&attack_kind!=artillery)value*=.75f;a.hp-=value;a.flash=.14f;
  if(a.hp>0)return;
  a.hp=0;blast(a.p,building(a.kind)?2:1);emit_sound(building(a.kind)?12:a.kind>=tank?13:3);
  int c=cell(a.p);if(terrain[c]!=water&&terrain[c]!=bridge&&!ore[c])terrain[c]=building(a.kind)?ruins:crater;
  if(a.team==1){++killed;credits+=a.kind>=hq?100:25;}else if(a.team==0)++lost;
  if(source>=0&&source<MAX&&e[source].hp>0&&e[source].team!=a.team&&e[source].team!=2&&a.team!=2){
   auto& killer=e[source];unsigned before=veteran_rank(killer);killer.kills=std::min(255,int(killer.kills)+1);
   if(veteran_rank(killer)>before){if(killer.hp<specs[killer.kind].hp)killer.hp=std::min(float(specs[killer.kind].hp),killer.hp+specs[killer.kind].hp*.2f);blast(killer.p,4);if(killer.team==0){say("部队晋升！火力增强，战地修复已完成");emit_sound(4);}}
  }
  power_update();
 }
 void fire(int id,int target){auto& a=e[id];bool admitted=false;
  for(auto& s:shots)if(!s.alive){s={};s.from=a.p;s.p=a.p;s.dest=e[target].p;s.target=int16_t(target|(unsigned(a.cargo)<<7));s.team=uint8_t((id+1)*2+a.team);s.kind=a.kind;s.damage=specs[a.kind].damage*(1+veteran_rank(a)*.12f)*(a.team==0?1+extra.damage*.12f:1);s.duration=a.kind==artillery?.85f:a.kind==rocket?.5f:.22f;s.alive=true;admitted=true;break;}
  if(!admitted)return;a.cool=specs[a.kind].reload;a.dir=std::atan2(e[target].p.y-a.p.y,e[target].p.x-a.p.x);unsigned cue=a.kind==rifle?6:a.kind==rocket?7:a.kind==artillery?9:8;if(sound<=2)sound=cue;else if(sound>=6&&sound<=9)sound=std::max(sound,cue);
 }
 void supply_tick(Entity& station,float dt){
  bool friendly=false,contested=false;for(auto& a:e)if(a.hp>0&&a.kind<harvester&&dist(a.p,station.p)<2.7f){friendly|=a.team==0;contested|=a.team==1;}
  if(!friendly){station.progress=std::max(0.f,station.progress-dt*.5f);return;}
  if(contested)return;station.progress+=dt;if(station.progress<5)return;
  credits+=280;for(auto& a:e)if(a.hp>0&&a.team==0&&a.kind<harvester&&dist(a.p,station.p)<4){if(a.hp<specs[a.kind].hp)a.hp=std::min(float(specs[a.kind].hp),a.hp+specs[a.kind].hp*.3f);blast(a.p,3);}
  station.hp=0;terrain[cell(station.p)]=3;blast(station.p,6);power_update();say("补给接收！矿石 +280，附近部队已维修",7);emit_sound(5);
 }
 bool strike(Vec p){if(strike_cd>0){say("支援还在冷却中");return false;}if(credits<180){say("空袭需要 180 矿石");return false;}if(!fog[cell(p)]){say("空袭需要可见目标");return false;}credits-=180;strike_cd=45;for(int i=0;i<MAX;++i)if(e[i].hp>0&&e[i].team==1&&dist(e[i].p,p)<3.8f)damage(i,260);for(int i=0;i<7;++i)blast({p.x+(int(random()%400)-200)*.01f,p.y+(int(random()%400)-200)*.01f},2);say("空中支援：雷鸣打击已抵达！");emit_sound(3);return true;}
 void repair(){for(auto& a:e)if(a.selected&&a.hp>0&&a.team==0){int cost=int((specs[a.kind].hp-a.hp)*.2f);if(cost<=0)continue;if(credits<cost){say("修复资金不足");return;}credits-=cost;a.hp=specs[a.kind].hp;blast(a.p,3);}say("维修完成");}
 void defense_wave(){bool slot=false;for(auto& a:e)slot|=a.hp<=0;if(!slot){next_wave=time+2;return;}++wave;
  auto spawn=wave_entry(wave);int units=std::min(12,3+wave);bool raid=wave%5==0;
  for(int j=0;j<units;++j){int kind=j%3==0?tank:j%3==1?rifle:rocket;if(wave>=4&&j==units-1)kind=artillery;if(raid&&j%3==0)kind=artillery;
   int id=add(kind,1,{spawn.x+j%3*.45f,spawn.y+j/3*.45f});if(id>=0){auto& a=e[id];a.goal=raid_goal(wave,j);a.step=a.p;a.moving=true;if(extra.map_rules){constexpr uint8_t ranks[]={0,2,5,9};a.kills=ranks[std::min(3,(wave-1)/8)];a.hp*=1+std::min(.6f,(wave/10)*.08f);}if(raid){a.hp*=1.25f;a.kills=std::max(uint8_t(2),a.kills);}}
  }
  char text[100];std::snprintf(text,sizeof text,raid?"第 %d 波：灰塔重装突袭！保护前哨电站！":"第 %d 波来袭！拦截接近矿线的敌军",wave);say(text,7);
 }
 void wave_spawn(){++wave;next_wave=time+std::max(24.f,55.f-wave*2.f-chapter*4.f);Vec spawn=wave_entry(wave);int units=std::min(7,2+wave/2+(chapter==2?1:chapter));for(int j=0;j<units;++j){int k=j%3==0?tank:j%3==1?rifle:rocket;if(chapter==2&&wave>=3&&j==units-1)k=artillery;int id=add(k,1,{spawn.x+(j%3)*.6f,spawn.y+(j/3)*.6f});if(id>=0){e[id].goal=raid_goal(wave,j);e[id].moving=true;e[id].step=e[id].p;}}say("侦察：敌方增援正向基地推进！",6);}
 [[gnu::noinline]] void tick(float dt){if(won||defeated)return;dt=std::clamp(dt,0.f,.1f);time+=dt;route_credits=6;autosave+=dt;notice_time=std::max(0.f,notice_time-dt);strike_cd=std::max(0.f,strike_cd-dt);reveal_timer-=dt;
  if(reveal_timer<=0){reveal();reveal_timer=.5f;}if(extra.mode){if(wave==int(extra.cleared)&&army(1)==0&&time>=next_wave)defense_wave();}else if(time>=next_wave&&count(hq,1))wave_spawn();research_tick(dt);
  if(job_count){jobs[0].left-=dt*(power_net<0?.45f:1.f);if(jobs[0].left<=0){int k=jobs[0].kind;int home=nearest({6,8},k<=rocket?barracks:factory,0);if(home<0){say("生产设施已损毁，资金已退回");credits+=specs[k].cost;}else {Vec pos={e[home].p.x+1.6f,e[home].p.y+1.6f};int id=add(k,0,pos);if(id>=0){blast(pos,3);if(e[home].moving&&open(e[home].goal)){e[id].goal=e[home].goal;e[id].step=pos;e[id].moving=true;}say("新部队已就绪");emit_sound(11);}else credits+=specs[k].cost;}for(int j=1;j<job_count;++j)jobs[j-1]=jobs[j];--job_count;}}
  for(int i=0;i<MAX;++i){auto& a=e[i];if(a.hp<=0)continue;a.flash=std::max(0.f,a.flash-dt);a.cool=std::max(0.f,a.cool-dt);
   if(a.kind==supply){supply_tick(a,dt);continue;}if(chapter==4&&a.team==2&&a.kind==harvester)continue;
   if(a.progress>0){a.progress-=dt*(power_net<0?.5f:1.f);if(a.progress<=0){a.progress=0;say("工程完成");if(a.kind==refinery){add(harvester,0,{a.p.x+1.5f,a.p.y+1.5f});}power_update();if(sound<3||(sound>=6&&sound<=9))emit_sound(11);}continue;}
   if(a.kind==harvester&&a.team==0){harvest(i,dt);continue;}
   if(a.kind==relay||a.kind==hq||a.kind==power||a.kind==refinery||a.kind==barracks||a.kind==factory)continue;
   int target=-1;float best=specs[a.kind].range;
   if(a.target>=0&&a.target<MAX&&e[a.target].hp>0&&e[a.target].team!=a.team&&e[a.target].team!=2){if(dist(a.p,e[a.target].p)<best+(building(e[a.target].kind)?1:0))target=a.target;else if(a.kind<hq){a.goal=e[a.target].p;a.moving=true;}}
   else a.target=-1;
   if(target<0)for(int j=0;j<MAX;++j){auto& b=e[j];if(b.hp<=0||b.team==a.team||(b.team==2&&(a.team!=1||chapter!=4||b.kind!=harvester))||(!fog[cell(b.p)]&&a.team==0))continue;float d=dist(a.p,b.p)-(building(b.kind)?.6f:0);if(d<best){target=j;best=d;}}
   bool powered=a.kind!=turret||(a.team==1?((chapter!=2&&chapter!=5)||count(power,1)>0):power_net>=0);
   if(target>=0&&powered){if(a.cool<=0)fire(i,target);}else if(a.moving&&a.kind<hq)move(i,dt);
  }
  for(auto& s:shots)if(s.alive){s.t+=dt;float f=std::min(1.f,s.t/s.duration);s.p={s.from.x+(s.dest.x-s.from.x)*f,s.from.y+(s.dest.y-s.from.y)*f};if(f>=1){
   s.alive=false;float value=s.damage;int victim_id=shot_target(s),source=shot_source(s);
   if(source<0||source>=MAX||e[source].hp<=0||e[source].team!=shot_team(s)||e[source].kind!=s.kind||e[source].cargo!=(unsigned(s.target)>>7))source=-1;
   if(victim_id>=0&&victim_id<MAX){auto& victim=e[victim_id];if(dist(victim.p,s.dest)<1.8f){if(s.kind==rifle&&victim.kind>=tank)value*=.32f;if(s.kind==rocket&&victim.kind==rifle)value*=.55f;damage(victim_id,value,source,s.kind);}}
   blast(s.dest,0);if(s.kind==artillery)for(int j=0;j<MAX;++j)if(j!=victim_id&&e[j].hp>0&&e[j].team!=shot_team(s)&&e[j].team!=2&&dist(e[j].p,s.dest)<1.7f)damage(j,value*.5f,source,artillery);
  }}
  for(auto& b:blasts)if(b.alive){b.age+=dt;float life=b.type==2?2.2f:b.type==1?1.1f:b.type==3?.7f:b.type==4?1.f:b.type==5?.16f:b.type==6?1.2f:.65f;if(b.age>life)b.alive=false;}
  if(!extra.mode&&chapter>=3){frontier_tick(dt);}
  else if(!extra.mode&&chapter==1){int r=nearest({20,12},relay,2);if(r>=0){bool near=false;for(auto& a:e)if(a.hp>0&&a.team==0&&a.kind<harvester&&dist(a.p,e[r].p)<3)near=true;if(near){capture+=dt;if(capture>=8){e[r].team=0;story_beat=true;mission.events|=1u<<5;say("苏弥：中继站已上线。坚持住，平民正在撤离！",8);}}}if(count(relay,0)){radio_time+=dt;if(radio_time>=90)won=true;}if(!count(relay,0)&&!count(relay,2))defeated=true;}
  else if(!extra.mode&&!count(hq,1)&&(chapter!=2||!count(power,1)))won=true;
  if(!count(hq,0))defeated=true;if(defeated)won=false;
  if(!extra.mode&&chapter==0&&!story_beat&&time>50){story_beat=true;mission.events|=1u<<6;say("苏弥：灰塔在用民用电网供能。我们必须查清真相。",8);}
  if(extra.mode&&!defeated&&wave>int(extra.cleared)&&army(1)==0)defense_clear();
  if(!extra.mode&&chapter==2&&!count(power,1)&&!story_beat){story_beat=true;mission.events|=1u<<7;for(auto& a:e)if(a.team==1&&a.kind==turret)a.cool=9999;say("林岚：敌方电网瘫痪！全军向灰塔推进！",8);}
 }
#include "frontier.inc"
};
}
