// SPDX-License-Identifier: MIT
#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <new>
#include <initializer_list>
namespace iron {
constexpr int N=64,MAX=100,CHAPTERS=6,squad_count=3;
struct Vec {float x=0,y=0;};
inline float distance_squared(Vec a,Vec b){float x=a.x-b.x,y=a.y-b.y;return x*x+y*y;}
inline float dist(Vec a,Vec b){return std::hypot(a.x-b.x,a.y-b.y);}
inline int cell(Vec p){return std::clamp(int(p.y),0,N-1)*N+std::clamp(int(p.x),0,N-1);}
inline Vec center(int c){return {float(c%N)+.5f,float(c/N)+.5f};}
enum Terrain:uint8_t {ground=0,woodland=1,water=2,cleared=3,crater=4,ruins=5,road=6,bridge=7,ridge=8,railbed=9};
enum Kind:uint8_t {rifle,rocket,tank,artillery,harvester,hq,power,refinery,barracks,factory,turret,relay,supply,patrol_boat,rotorcraft,commander,shipyard,helipad,aa_tower,kind_count};
// Keep published kind IDs stable. Domain and building checks are independent of ID order.
constexpr bool combat_kind(int k){return (k>=rifle&&k<harvester)||k==patrol_boat||k==rotorcraft||k==commander;}
constexpr bool unit_kind(int k){return combat_kind(k)||k==harvester;}
constexpr bool land_combat(int k){return combat_kind(k)&&k!=patrol_boat&&k!=rotorcraft;}
constexpr bool construction_kind(int k){return (k>=power&&k<=turret)||(k>=shipyard&&k<=aa_tower);}
constexpr bool defence_kind(int k){return k==turret||k==aa_tower;}
constexpr int producer_for(int k){
 if(k==patrol_boat)return shipyard;
 if(k==rotorcraft)return helipad;
 if(k==commander)return hq;
 return k<=rocket?barracks:factory;
}
constexpr bool production_building(int k){return k==hq||k==barracks||k==factory||k==shipyard||k==helipad;}
constexpr bool can_target(int attacker,int victim){
 if(attacker==aa_tower)return victim==rotorcraft;
 return victim!=rotorcraft||attacker==rotorcraft||attacker==rocket;
}
constexpr int craft_altitude=42;
constexpr float support_cooldown=44.f,support_radius=5.f;
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
 {"战地补给站",0,600,0,0,0,1,1,0},
 {"浅潮巡逻艇",290,350,2.1f,5.4f,37,1.25f,6,0},
 {"雨燕旋翼机",490,300,3.0f,5.6f,34,1.0f,9,0},
 {"指挥官林岚",520,330,1.5f,4.6f,31,.8f,11,0},
 {"河岸船坞",360,820,0,0,0,1,8,-15},
 {"旋翼机坪",430,850,0,0,0,1,9,-25},
 {"防空塔",260,610,0,7.4f,58,1.15f,6,-15}
};
static_assert(std::size(specs)==kind_count);
struct Entity {Vec p{},goal{},step{};float hp=0,cool=0,progress=0,dir=0,flash=0,stuck=0;int16_t target=-1;uint8_t kind=0,team=0,cargo=0,kills=0;bool selected=false,moving=false;};
constexpr int retreat_order=-3,escort_order_base=-4;
inline int escort_id(const Entity& a){return a.target<=escort_order_base?escort_order_base-a.target:-1;}
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
struct Job {uint8_t kind=0,producer=0;float left=0,total=0;};
struct Expansion {uint32_t mode=0,seed=0,cleared=0;uint8_t damage=0,armor=0,research=0,map_rules=0;float research_left=0;};
struct Mission {uint32_t events=0,read_events=0;uint8_t stage=0,delivered=0;std::array<uint8_t,3> waypoint{};std::array<int16_t,3> convoy{{-1,-1,-1}};float support_left=0;};
enum class GroundMaterial : uint8_t { grass, red_gravel, marsh, scree, asphalt };
struct CampaignMap {
 int side, credits, first_wave;
 GroundMaterial material;
 Vec home, front;
 std::array<Vec, 2> entries;
 const char* arrival;
};
inline constexpr CampaignMap campaign_maps[] = {
 {36, 1300, 84, GroundMaterial::red_gravel, {7,25}, {11,22}, {{{31,7},{31,25}}}, "林岚：西南矿场已经复工。沿运矿路北上，夺回灰塔电台。"},
 {44, 1500, 105, GroundMaterial::marsh, {36,24}, {32,24}, {{{5,8},{4,34}}}, "苏弥：电台在西岸。抢下公路桥，给平民留出撤离的时间。"},
 {48, 1600, 115, GroundMaterial::asphalt, {23,7}, {23,12}, {{{6,35},{42,39}}}, "林岚：两座电站分在东西两侧。先断电，再进入南区。"},
 {54, 1750, 125, GroundMaterial::scree, {9,40}, {14,37}, {{{48,10},{48,42}}}, "苏弥：北隘口和东侧矿场各有一座电台。先拿到两段行车记录。"},
 {60, 1900, 135, GroundMaterial::grass, {51,10}, {47,16}, {{{16,12},{22,47}}}, "何舟：车队在东北货场。先派装甲过渠，接通西岸的引导台。"},
 {64, 2100, 150, GroundMaterial::asphalt, {53,53}, {52,48}, {{{7,12},{48,7}}}, "林岚：沿东南大道推进，逐段接管调度站。医院的线路必须保住。"},
};
struct World {
 std::array<Entity,MAX> e{};std::array<Shot,64> shots{};std::array<Blast,48> blasts{};
 std::array<uint8_t,N*N> terrain{},ore{},fog{},seen{},obstacles{};std::array<Job,6> jobs{};
 uint32_t rng=83711;int chapter=0,credits=1100,power_net=0,job_count=0,wave=0,killed=0,lost=0,earned=0;
 float time=0,next_wave=58,strike_cd=0,capture=0,autosave=0,notice_time=0,reveal_timer=0,radio_time=0;
 bool won=false,defeated=false,story_beat=false;char notice[128]{};unsigned sound=0;Expansion extra{};
 int side=28;Mission mission{};unsigned route_credits=6;Job construction{};
 std::array<uint8_t,MAX> squads{};
 void emit_sound(unsigned cue){if(sound!=12&&(sound!=13||cue==12))sound=cue;}
 uint32_t random(){rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;return rng;}
 void say(const char* s,float duration=5){std::snprintf(notice,sizeof notice,"%s",s);notice_time=duration;}
 bool building(int k)const{return !unit_kind(k);}
 int count(int k,int team=0)const{int n=0;for(auto& a:e)n+=a.hp>0&&a.team==team&&a.kind==k;return n;}
 int army(int team=0)const{int n=0;for(auto& a:e)n+=a.hp>0&&a.team==team&&combat_kind(a.kind);return n;}
 bool open(Vec p,int ignore=-1)const{if(p.x<1||p.y<1||p.x>=side-1||p.y>=side-1||(terrain[cell(p)]==water||terrain[cell(p)]==ridge))return false;for(int i=0;i<MAX;++i){auto& a=e[i];if(i!=ignore&&a.hp>0&&building(a.kind)&&distance_squared(a.p,p)<(defence_kind(a.kind)?.4225f:1.44f))return false;}return true;}
 int add(int kind,int team,Vec p){for(int i=0;i<MAX;++i)if(e[i].hp<=0){squads[i]=0;e[i]={};auto& a=e[i];a.p=a.goal=a.step=p;a.hp=specs[kind].hp;a.kind=kind;a.team=team;if(combat_kind(kind)||defence_kind(kind))a.cargo=uint8_t(random()%255+1);a.dir=team?3.14f:0;return i;}return -1;}
 void init(int ch) {
  new(this) World{};
  chapter = std::clamp(ch, 0, CHAPTERS-1);
  init_campaign();
 }
 // Defense keeps its seeded 28-cell battlefield and its separate career.
 void init_defense(uint32_t seed) {
  new(this) World{};
  extra.mode = 1;
  extra.map_rules = 1;
  extra.seed = seed ? seed : 83711;
  credits = 1500;
  next_wave = 35;
  terrain.fill(water);
  // Reproduce the starter deposits used by the published defense seeds.
  for (int y=0;y<side;++y) for (int x=0;x<side;++x) {
   random();
   bool mine=(x>=3&&x<=6&&y>=13&&y<=16)||(x>=19&&x<=22&&y>=6&&y<=9)||(x>=7&&x<=10&&y>=21&&y<=23);
   if(mine) ore[y*N+x]=180+random()%76;
  }
  for(int y=20;y<=21;++y)for(int x=16;x<=17;++x)ore[y*N+x]=160+unsigned(x*19+y*11)%65;
  deploy(0, {{hq,{5.5f,8.5f}}, {power,{3.5f,7}}, {refinery,{4,11}}, {barracks,{8,8}},
             {harvester,{5,12.5f}}, {tank,{8.5f,11}}, {tank,{9.5f,12}}, {rifle,{8,13}}, {rocket,{10.5f,11.5f}}});
  rng = extra.seed;
  for (int y=1;y<side-1;++y) for (int x=1;x<side-1;++x) {
   int c=y*N+x;
   terrain[c]=ore[c]?ground:(random()%13==0?woodland:ground);
  }
  for(int y=1;y<side-1;++y)if(!defense_crossing(y))for(int x=17;x<=18;++x){terrain[y*N+x]=water;ore[y*N+x]=0;}
  add(factory,0,{7,5});add(power,0,{3,4});add(turret,0,{10,9});
  add(supply,2,{13,8});add(supply,2,{9,18});add(supply,2,{19,16});
  for(int y=18;y<21;++y)for(int x=13;x<16;++x)ore[y*N+x]=200+random()%55;
  build_defense_landscape();say("苏弥：守住前哨。每次击退敌军，补给队就能靠近。",8);power_update();reveal();
 }
 // Terrain is stored in the existing byte grid; published save prefixes stay intact.
 void build_defense_landscape(){
  for(int y=2;y<side-2;++y){auto crossing=[&](int row){return defense_crossing(row);};if(crossing(y-1)||crossing(y)||crossing(y+1))continue;for(int side=0;side<2;++side){int x=side?19:16,c=y*N+x;unsigned hash=x*97+y*53+extra.seed;if(hash%4!=0||ore[c]||terrain[c]!=ground)continue;bool occupied=false;for(auto& a:e)if(a.hp>0&&dist(a.p,center(c))<2.5f){occupied=true;break;}if(!occupied)terrain[c]=water;}}
  auto rocks=[&](int left,int top,int width,int height){for(int y=top;y<top+height;++y)for(int x=left;x<left+width;++x){int c=y*N+x;if(terrain[c]==water||ore[c])continue;bool occupied=false;for(auto& a:e)if(a.hp>0&&dist(a.p,center(c))<2.3f){occupied=true;break;}if(!occupied)terrain[c]=ridge;}};
  rocks(13,3,2,3);rocks(12,12,2,3);rocks(20,18,2,3);
  auto paving=[&](int x,int y){if(x<=0||y<=0||x>=side-1||y>=side-1)return;int c=y*N+x;if(!ore[c]&&terrain[c]!=water)terrain[c]=road;};
  for(int x=2;x<=25;++x)for(int y=9;y<=10;++y)paving(x,y);for(int y=3;y<=25;++y)for(int x=10;x<=11;++x)paving(x,y);
  for(int x=8;x<=24;++x)for(int y=17;y<=18;++y)paving(x,y);for(int y=9;y<=23;++y)for(int x=20;x<=21;++x)paving(x,y);
  // Soft woodland patches leave roads, mine cells and building approaches clear.
  for(Vec patch:{Vec{9,16},Vec{16,6},Vec{23,13},Vec{8,21}})for(int y=int(patch.y)-2;y<=int(patch.y)+2;++y)for(int x=int(patch.x)-2;x<=int(patch.x)+2;++x){int c=y*N+x;Vec p=center(c);if(terrain[c]!=ground||ore[c]||dist(p,patch)>2.2f)continue;bool occupied=false;for(auto& a:e)if(a.hp>0&&building(a.kind)&&dist(a.p,p)<2.4f){occupied=true;break;}if(!occupied)terrain[c]=woodland;}
  for(int y=1;y<side-1;++y)if(defense_crossing(y))for(int x=17;x<=18;++x)terrain[y*N+x]=bridge,ore[y*N+x]=0;
  // Forward deposits reward opening a second mine without replacing the safe starter ore.
  for(int y=20;y<=21;++y)for(int x=16;x<=17;++x){int c=y*N+x;if(terrain[c]==ground||terrain[c]==woodland)ore[c]=160+unsigned(x*19+y*11+chapter*23)%65;}
 }
 bool paved(Vec p)const{return terrain[cell(p)]==road||terrain[cell(p)]==bridge;}
 bool in_cover(const Entity& a)const{return extra.map_rules&&(a.kind<=rocket||a.kind==commander)&&(terrain[cell(a.p)]==woodland||terrain[cell(a.p)]==crater||terrain[cell(a.p)]==ruins);}
 float move_speed(const Entity& a)const{float speed=specs[a.kind].speed;if(a.kind==rotorcraft||a.kind==patrol_boat)return speed;bool vehicle=a.kind>=tank&&a.kind<=harvester;if(a.kind==harvester&&a.cargo)speed=1.42f;if(paved(a.p))speed*=vehicle?1.2f:1.05f;else if(terrain[cell(a.p)]==woodland)speed*=vehicle?.82f:.95f;else if(terrain[cell(a.p)]==crater||terrain[cell(a.p)]==ruins)speed*=vehicle?.78f:.9f;return speed;}
 Vec wave_entry(int next) const {
  if (authored_map()) return campaign_maps[chapter].entries[(next-1)&1];
  if (extra.mode) {
   constexpr Vec edges[]={{23,5},{24,22},{5,24}};
   return edges[(extra.seed+unsigned(next))%3];
  }
  if (chapter>=3) return next%2?Vec{float(side-5),6}:Vec{float(side-5),float(side-6)};
  return chapter==1?Vec{23,24}:Vec{24,22};
 }
 const char* wave_direction(int next) const {
  auto p=wave_entry(next);
  if (!authored_map()) return p.x<10?"南路":p.y<10?"东北":"东南";
  int east = p.x > side*2.f/3, west = p.x < side/3.f;
  if (p.y < side/3.f) return west?"西北":east?"东北":"北面";
  if (p.y > side*2.f/3) return west?"西南":east?"东南":"南面";
  return west?"西面":east?"东面":"中路";
 }
 Vec raid_goal(int next,int unit) const {
  if (chapter==4 && !extra.mode && unit%2) {
   for (int id : mission.convoy) if (active_convoy(id)) return e[id].p;
  }
  if (chapter==1 && !extra.mode) {
   int id = nearest(base_position(),relay,0);
   if (id>=0) return e[id].p;
  }
  if (authored_map()) {
   const auto& map=campaign_maps[chapter];
   if (next>=2 && unit%3==1) {
    int id=nearest(map.home,next%2?refinery:factory,0);
    if (id>=0) return e[id].p;
   }
   return map.front;
  }
  if(extra.map_rules&&next>=2&&unit%3==1)return next%2?Vec{5,13}:Vec{7,5};
  return {7,10};
 }
 GroundMaterial material() const {
  return authored_map() ? campaign_maps[chapter].material : GroundMaterial::grass;
 }
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
  int base=nearest({6,9},hq,0);if(base>=0)for(auto& a:e)if(a.hp>0&&a.team==0&&unit_kind(a.kind)&&dist(a.p,e[base].p)<8){if(a.hp<specs[a.kind].hp)a.hp=std::min(float(specs[a.kind].hp),a.hp+specs[a.kind].hp*.12f);blast(a.p,3);}
  if(wave%3==0){Vec station=wave%2?Vec{15,16}:Vec{18,8};if(open(station)&&add(supply,2,station)>=0)power_update();}
  char text[120];std::snprintf(text,sizeof text,"第 %d 波已击退！补给 +%d，前哨维修完成",wave,reward);say(text,8);emit_sound(5);
 }
 void power_update(){
  for(int c=0;c<N*N;++c)obstacles[c]=terrain[c]==water||terrain[c]==ridge;
  power_net=0;
  for(const auto& a:e){
   if(a.hp<=0||!building(a.kind))continue;
   if(a.team==0&&a.progress<=0)power_net+=specs[a.kind].supply;
   int x=int(a.p.x),y=int(a.p.y);
   float limit=defence_kind(a.kind)?.4225f:1.44f;
   for(int yy=std::max(0,y-1);yy<=std::min(N-1,y+1);++yy)
    for(int xx=std::max(0,x-1);xx<=std::min(N-1,x+1);++xx){
     int c=yy*N+xx;
     if(distance_squared(a.p,center(c))<limit)obstacles[c]=1;
    }
  }
 }
 int ready_count(int k)const{int n=0;for(auto& a:e)n+=a.hp>0&&a.team==0&&a.kind==k&&a.progress<=0;return n;}
 bool unlocked(int k)const{
  if(unit_kind(k))return ready_count(producer_for(k))>0;
  if(k==factory)return ready_count(barracks)&&ready_count(refinery);
  if(k==shipyard)return ready_count(refinery)>0;
  if(k==helipad)return ready_count(factory)>0;
  if(k==turret||k==aa_tower)return ready_count(barracks)>0;
  return construction_kind(k);
 }
 const char* blocked(int k)const{
  if(!unlocked(k))return "需要对应生产设施";
  if(k==commander){
   if(count(commander))return "林岚已在战场";
   for(int i=0;i<job_count;++i)if(jobs[i].kind==commander)return "林岚正在集结";
  }
  if(credits<specs[k].cost)return "矿石不足，等待采矿车返航";
  if(job_count>=6)return "生产队列已满";
  int roster=army();
  for(int i=0;i<job_count;++i)roster+=combat_kind(jobs[i].kind);
  if(roster>=36&&combat_kind(k))return "部队已达 36 人上限";
  return nullptr;
 }
 void select_all(){for(auto& a:e)a.selected=a.hp>0&&a.team==0&&combat_kind(a.kind);say("已选择全部作战部队");}
 int selected()const{int n=0;for(auto& a:e)n+=a.hp>0&&a.selected;return n;}
 void select_one(int id,bool append=false){for(int i=0;i<MAX;++i)if(!append)e[i].selected=false;if(id>=0&&id<MAX&&e[id].hp>0&&e[id].team==0)e[id].selected=true;}
 int hit(Vec p,float radius=1.f)const{int found=-1;float best=radius;for(int i=0;i<MAX;++i){auto& a=e[i];if(a.hp<=0||(!fog[cell(a.p)]&&a.team!=0))continue;float d=dist(p,a.p);if(d<best){best=d;found=i;}}return found;}
 void stop_selected(){int n=0;for(auto& a:e)if(a.hp>0&&a.selected&&a.team==0&&combat_kind(a.kind)){a.goal=a.step=a.p;a.moving=false;a.target=-1;++n;}say(n?"编队就地警戒，自动还击":"请先选择作战部队");}
 Vec formation_goal(Vec p,int index,int kind=rifle)const{
  float offset_x=((index%4)-1.5f)*.65f,offset_y=(index/4)*.65f;
  Vec goal{std::clamp(p.x+offset_x,1.2f,side-1.5f),std::clamp(p.y+offset_y,1.2f,side-1.5f)};
  return open_for(kind,goal)?goal:p;
 }
 void order(Vec p,int target=-1){
  if(target>=MAX)target=-1;
  int n=0,rallies=0;
  for(auto& a:e){
   if(a.hp<=0||!a.selected||a.team!=0)continue;
   if(production_building(a.kind)){
    int kind=a.kind==shipyard?patrol_boat:a.kind==helipad?rotorcraft:rifle;
    if(target<0&&open_for(kind,p)){a.goal=p;a.moving=true;++rallies;}
    continue;
   }
   if(!unit_kind(a.kind))continue;
   if(target<0&&a.kind==patrol_boat&&!open_for(a.kind,p))continue;
   if(target>=0&&!can_target(a.kind,e[target].kind))continue;
   a.goal=formation_goal(p,n++,a.kind);a.step=a.p;a.stuck=0;a.moving=true;a.target=target;
  }
  if(n){say(target>=0?"集中火力，攻击目标！":"收到，编队前进。");emit_sound(1);}
  else if(rallies){say("集结点已设置，新部队会自动前往");emit_sound(1);}
  else if(selected())say("检查目标和可通行的地形");
 }
 [[gnu::noinline]] void reveal(){
  fog.fill(0);
  for(const auto& a:e)if(a.hp>0&&a.team==0){
   int radius=a.kind==rotorcraft?9:a.kind==hq?7:6;
   int left=std::max(0,int(a.p.x)-radius),right=std::min(side-1,int(a.p.x)+radius);
   int top=std::max(0,int(a.p.y)-radius),bottom=std::min(side-1,int(a.p.y)+radius);
   for(int y=top;y<=bottom;++y){
    float dy=y+.5f-a.p.y,remaining=radius*radius-dy*dy;
    for(int x=left;x<=right;++x){
     float dx=x+.5f-a.p.x;
     if(dx*dx<remaining)fog[y*N+x]=seen[y*N+x]=1;
    }
   }
  }
 }
 int nearest(Vec p,int kind,int team)const{
  float best=999.f*999.f;int id=-1;
  for(int i=0;i<MAX;++i){
   auto& a=e[i];
   if(a.hp<=0||a.progress>0||a.kind!=kind||a.team!=team)continue;
   float d=distance_squared(p,a.p);
   if(d<best){best=d;id=i;}
  }
  return id;
 }
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
 // The unobstructed x-then-y route is the first shortest route in BFS order.
 // Check it before opening the full search, especially for long urban streets.
 int clear_route(int src,int dest)const{
  int x=src%N,y=src/N,tx=dest%N,ty=dest/N;
  if(x<1||y<1||x>=side-1||y>=side-1||tx<1||ty<1||tx>=side-1||ty>=side-1||obstacles[dest])return -1;
  const int dx=tx>x?1:-1,dy=ty>y?1:-1;
  int first=x!=tx?src+dx:src+dy*N;
  while(x!=tx){x+=dx;if(obstacles[y*N+x])return -1;}
  while(y!=ty){y+=dy;if(obstacles[y*N+x])return -1;}
  return first;
 }

 [[gnu::noinline]] Vec path(Vec from,Vec to,int id)const{
  int src=cell(from),dest=cell(to);if(src==dest)return to;
  int kind=id>=0&&id<MAX?e[id].kind:rifle;
  if(kind==rotorcraft)return to;
  bool naval=kind==patrol_boat;
  int first=naval?-1:clear_route(src,dest);
  if(first>=0)return center(first);
  constexpr int offset[]={1,-1,N,-N};
  std::array<uint8_t,N*N> parent{};
  std::array<int16_t,N*N> queue;
  int head=0,tail=1,closest=src;
  queue[0]=src;parent[src]=5;
  while(head<tail&&closest==src){
   int c=queue[head++],x=c%N,y=c/N;
   for(int d=0;d<4;++d){
    int xx=x+(d==0)-(d==1),yy=y+(d==2)-(d==3);
    if(xx<1||yy<1||xx>=side-1||yy>=side-1)continue;
    int next=c+offset[d];
    if(parent[next]||(naval?!waterway_cell(next):bool(obstacles[next])))continue;
    parent[next]=uint8_t(d+1);
    if(next==dest){closest=dest;break;}
    queue[tail++]=next;
   }
  }
  if(closest==src){
   // An occupied or disconnected goal can still be approached from the nearest
   // reachable cell. Defer this comparison until there is no exact route.
   float best=distance_squared(from,to);
   for(int i=1;i<tail;++i){
    int c=queue[i];
    float distance=distance_squared(center(c),to);
    if(distance<best){best=distance;closest=c;}
   }
  }
  if(closest==src)return from;
  while(closest-offset[parent[closest]-1]!=src)closest-=offset[parent[closest]-1];
  return center(closest);
 }

 [[gnu::noinline]] void move(int id,float dt){auto& a=e[id];if(dist(a.p,a.goal)<.18f){a.moving=false;return;}if(a.kind==rotorcraft)a.step=a.goal;else if(dist(a.p,a.step)<.13f||!open_for(a.kind,a.step,id)){if(!route_credits)return;--route_credits;a.step=path(a.p,a.goal,id);}float d=dist(a.p,a.step);if(d<.02f){a.stuck+=dt;if(a.stuck>2)a.moving=false;return;}a.stuck=0;float heading=std::atan2(a.step.y-a.p.y,a.step.x-a.p.x),alignment=1;if((a.kind>=tank&&a.kind<=harvester)||a.kind==patrol_boat||a.kind==rotorcraft){float difference=std::remainder(heading-a.dir,6.2831853f),turn=(a.kind==rotorcraft?8.f:a.kind==patrol_boat?5.f:a.kind==artillery?2.6f:a.kind==harvester?4.2f:3.5f)*dt;a.dir=std::remainder(a.dir+std::clamp(difference,-turn,turn),6.2831853f);alignment=std::max(0.f,std::cos(std::remainder(heading-a.dir,6.2831853f)));}else a.dir=heading;float v=std::min(d,move_speed(a)*alignment*dt);a.p.x+=(a.step.x-a.p.x)/d*v;a.p.y+=(a.step.y-a.p.y)/d*v;
  // Soft separation makes formations readable without deadlocking a crossing.
  for(int j=0;j<id;++j)if(e[j].hp>0&&unit_kind(e[j].kind)&&(e[j].kind==patrol_boat)==(a.kind==patrol_boat)&&(e[j].kind==rotorcraft)==(a.kind==rotorcraft)){float d2=distance_squared(a.p,e[j].p);if(d2>.01f*.01f&&d2<.38f*.38f){float dd=std::sqrt(d2);Vec n={a.p.x+(a.p.x-e[j].p.x)/dd*dt*.25f,a.p.y+(a.p.y-e[j].p.y)/dd*dt*.25f};if(open_for(a.kind,n,id))a.p=n;}}
 }
 void blast(Vec p,int type=0){for(auto& b:blasts)if(!b.alive){b={p,0,uint8_t(type),true};return;}}
 void damage(int id,float value,int source=-1,int weapon=-1){
  if(id<0||id>=MAX||e[id].hp<=0)return;auto& a=e[id];if(!extra.mode&&chapter==5&&a.team==1&&a.kind==hq&&mission.stage<2)return;if(a.team==0)value*=1-extra.armor*.10f;int attack_kind=weapon>=0?weapon:source>=0&&source<MAX?int(e[source].kind):-1;if(in_cover(a)&&attack_kind>=0&&attack_kind!=artillery)value*=.75f;a.hp-=value;a.flash=.14f;
  if(a.hp>0)return;
  a.hp=0;if(a.kind==harvester&&a.team!=1)finish_escort_target(id);
  blast(a.p,building(a.kind)?2:1);emit_sound(building(a.kind)?12:a.kind>=tank?13:3);
  int c=cell(a.p);
  bool was_ridge=terrain[c]==ridge;
  if(a.kind!=rotorcraft&&terrain[c]!=water&&terrain[c]!=bridge&&terrain[c]!=railbed&&!ore[c])terrain[c]=building(a.kind)?ruins:crater;
  squads[id]=0;
  if(a.team==1){++killed;credits+=building(a.kind)?100:25;}else if(a.team==0){++lost;if(production_building(a.kind))refund_destroyed_facility(id);}
  if(source>=0&&source<MAX&&e[source].hp>0&&e[source].team!=a.team&&e[source].team!=2&&a.team!=2){
   auto& killer=e[source];unsigned before=veteran_rank(killer);killer.kills=std::min(255,int(killer.kills)+1);
   if(veteran_rank(killer)>before){if(killer.hp<specs[killer.kind].hp)killer.hp=std::min(float(specs[killer.kind].hp),killer.hp+specs[killer.kind].hp*.2f);blast(killer.p,4);if(killer.team==0){say("部队晋升！火力增强，战地修复已完成");emit_sound(4);}}
  }
  if(building(a.kind)||(was_ridge&&terrain[c]!=ridge))power_update();
 }
 void fire(int id,int target){auto& a=e[id];if(!can_target(a.kind,e[target].kind))return;bool admitted=false;
  for(auto& s:shots)if(!s.alive){s={};s.from=a.p;s.p=a.p;s.dest=e[target].p;s.target=int16_t(target|(unsigned(a.cargo)<<7));s.team=uint8_t((id+1)*2+a.team);s.kind=a.kind;s.damage=specs[a.kind].damage*(1+veteran_rank(a)*.12f)*(a.team==0?1+extra.damage*.12f:1);s.duration=a.kind==artillery?.85f:(a.kind==rocket||a.kind==aa_tower||a.kind==rotorcraft)?.5f:.22f;s.alive=true;admitted=true;break;}
  if(!admitted)return;a.cool=specs[a.kind].reload;a.dir=std::atan2(e[target].p.y-a.p.y,e[target].p.x-a.p.x);unsigned cue=(a.kind==rifle||a.kind==commander)?6:(a.kind==rocket||a.kind==aa_tower||a.kind==rotorcraft)?7:a.kind==artillery?9:8;if(sound<=2)sound=cue;else if(sound>=6&&sound<=9)sound=std::max(sound,cue);
 }
 void supply_tick(Entity& station,float dt){
  bool friendly=false,contested=false;for(auto& a:e)if(a.hp>0&&land_combat(a.kind)&&dist(a.p,station.p)<2.7f){friendly|=a.team==0;contested|=a.team==1;}
  if(!friendly){station.progress=std::max(0.f,station.progress-dt*.5f);return;}
  if(contested)return;station.progress+=dt;if(station.progress<5)return;
  credits+=280;for(auto& a:e)if(a.hp>0&&a.team==0&&combat_kind(a.kind)&&dist(a.p,station.p)<4){if(a.hp<specs[a.kind].hp)a.hp=std::min(float(specs[a.kind].hp),a.hp+specs[a.kind].hp*.3f);blast(a.p,3);}
  station.hp=0;terrain[cell(station.p)]=3;blast(station.p,6);power_update();say("补给接收！矿石 +280，附近部队已维修",7);emit_sound(5);
 }
 bool strike(Vec p){if(strike_cd>0){say("支援还在冷却中");return false;}if(credits<180){say("空袭需要 180 矿石");return false;}if(!fog[cell(p)]){say("空袭需要可见目标");return false;}credits-=180;strike_cd=45;for(int i=0;i<MAX;++i)if(e[i].hp>0&&e[i].team==1&&e[i].kind!=rotorcraft&&dist(e[i].p,p)<3.8f)damage(i,260);for(int i=0;i<7;++i)blast({p.x+(int(random()%400)-200)*.01f,p.y+(int(random()%400)-200)*.01f},2);say("空中支援：雷鸣打击已抵达！");emit_sound(3);return true;}
 void repair(){for(auto& a:e)if(a.selected&&a.hp>0&&a.team==0){int cost=int((specs[a.kind].hp-a.hp)*.2f);if(cost<=0)continue;if(credits<cost){say("修复资金不足");return;}credits-=cost;a.hp=specs[a.kind].hp;blast(a.p,3);}say("维修完成");}
 void defense_wave(){bool slot=false;for(auto& a:e)slot|=a.hp<=0;if(!slot){next_wave=time+2;return;}++wave;
  auto spawn=wave_entry(wave);int units=std::min(12,3+wave);bool raid=wave%5==0;
  for(int j=0;j<units;++j){int kind=j%3==0?tank:j%3==1?rifle:rocket;if(wave>=4&&j==units-1)kind=artillery;if(raid&&j%3==0)kind=artillery;if(wave>=6&&wave%3==0&&j==units-1)kind=rotorcraft;
   int id=add(kind,1,{spawn.x+j%3*.45f,spawn.y+j/3*.45f});if(id>=0){auto& a=e[id];a.goal=raid_goal(wave,j);a.step=a.p;a.moving=true;if(extra.map_rules){constexpr uint8_t ranks[]={0,2,5,9};a.kills=ranks[std::min(3,(wave-1)/8)];a.hp*=1+std::min(.6f,(wave/10)*.08f);}if(raid){a.hp*=1.25f;a.kills=std::max(uint8_t(2),a.kills);}}
  }
  char text[100];std::snprintf(text,sizeof text,raid?"第 %d 波：灰塔重装突袭！保护前哨电站！":"第 %d 波来袭！拦截接近矿线的敌军",wave);say(text,7);
 }
 void wave_spawn(){++wave;next_wave=time+(authored_map()?std::max(42.f,68.f-wave*3.f):std::max(24.f,55.f-wave*2.f-chapter*4.f));Vec spawn=wave_entry(wave);int units=std::min(7,2+wave/2+(chapter==2?1:chapter));for(int j=0;j<units;++j){int k=j%3==0?tank:j%3==1?rifle:rocket;if(chapter==2&&wave>=3&&j==units-1)k=artillery;int id=add(k,1,{spawn.x+(j%3)*.6f,spawn.y+(j/3)*.6f});if(id>=0){e[id].goal=raid_goal(wave,j);e[id].moving=true;e[id].step=e[id].p;}}say("侦察：敌方增援正向基地推进！",6);}
 [[gnu::noinline]] void tick(float dt){if(won||defeated)return;dt=std::clamp(dt,0.f,.1f);time+=dt;route_credits=6;autosave+=dt;notice_time=std::max(0.f,notice_time-dt);strike_cd=std::max(0.f,strike_cd-dt);reveal_timer-=dt;
  if(reveal_timer<=0){reveal();reveal_timer=.5f;}if(extra.mode){if(wave==int(extra.cleared)&&army(1)==0&&time>=next_wave)defense_wave();}else if(time>=next_wave&&count(hq,1))wave_spawn();research_tick(dt);
  mission.support_left=std::max(0.f,mission.support_left-dt);
  construction_tick(dt);training_tick(dt);
  for(int i=0;i<MAX;++i){auto& a=e[i];if(a.hp<=0)continue;a.flash=std::max(0.f,a.flash-dt);a.cool=std::max(0.f,a.cool-dt);
   if(a.kind==supply){supply_tick(a,dt);continue;}if(chapter==4&&a.team==2&&a.kind==harvester)continue;
   if(a.progress>0){a.progress-=dt*(power_net<0?.5f:1.f);if(a.progress<=0){a.progress=0;say("工程完成");if(a.kind==refinery){add(harvester,0,{a.p.x+1.5f,a.p.y+1.5f});}power_update();if(sound<3||(sound>=6&&sound<=9))emit_sound(11);}continue;}
   if(a.kind==harvester&&a.team==0){harvest(i,dt);continue;}
   if(building(a.kind)&&!defence_kind(a.kind))continue;
   if(a.team==0&&combat_kind(a.kind)){
    if(a.target==retreat_order){
     if(a.moving)move(i,dt);
     if(!a.moving)a.target=-1;
     continue;
    }
    if(escort_id(a)>=0)update_escort(a,dt);
   }
   int target=-1;float best=specs[a.kind].range;
   if(a.target>=0&&a.target<MAX&&e[a.target].hp>0&&e[a.target].team!=a.team&&e[a.target].team!=2&&can_target(a.kind,e[a.target].kind)){if(dist(a.p,e[a.target].p)<best+(building(e[a.target].kind)?1:0))target=a.target;else if(unit_kind(a.kind)){a.goal=e[a.target].p;a.moving=true;}}
   else if(escort_id(a)<0)a.target=-1;
   if(target<0)for(int j=0;j<MAX;++j){auto& b=e[j];if(b.hp<=0||b.team==a.team||!can_target(a.kind,b.kind)||(b.team==2&&(a.team!=1||chapter!=4||b.kind!=harvester))||(!fog[cell(b.p)]&&a.team==0))continue;float pad=building(b.kind)?.6f:0,range=best+pad;float d2=distance_squared(a.p,b.p);if(range>0&&d2<range*range){target=j;best=std::sqrt(d2)-pad;}}
   bool powered=!defence_kind(a.kind)||(a.team==1?((chapter!=2&&chapter!=5)||count(power,1)>0):power_net>=0);
   if(target>=0&&powered){if(a.cool<=0)fire(i,target);}else if(a.moving&&unit_kind(a.kind))move(i,dt);
  }
  for(auto& s:shots)if(s.alive){s.t+=dt;float f=std::min(1.f,s.t/s.duration);s.p={s.from.x+(s.dest.x-s.from.x)*f,s.from.y+(s.dest.y-s.from.y)*f};if(f>=1){
   s.alive=false;float value=s.damage;int victim_id=shot_target(s),source=shot_source(s);
   if(source<0||source>=MAX||e[source].hp<=0||e[source].team!=shot_team(s)||e[source].kind!=s.kind||e[source].cargo!=(unsigned(s.target)>>7))source=-1;
   if(victim_id>=0&&victim_id<MAX){auto& victim=e[victim_id];if(can_target(s.kind,victim.kind)&&dist(victim.p,s.dest)<1.8f){if(s.kind==rifle&&victim.kind>=tank&&victim.kind!=commander)value*=.32f;if(s.kind==rocket&&(victim.kind==rifle||victim.kind==commander))value*=.55f;damage(victim_id,value,source,s.kind);}}
   blast(s.dest,0);if(s.kind==artillery)for(int j=0;j<MAX;++j)if(j!=victim_id&&e[j].hp>0&&e[j].team!=shot_team(s)&&e[j].team!=2&&e[j].kind!=rotorcraft&&dist(e[j].p,s.dest)<1.7f)damage(j,value*.5f,source,artillery);
  }}
  for(auto& b:blasts)if(b.alive){b.age+=dt;float life=b.type==2?2.2f:b.type==1?1.1f:b.type==3?.7f:b.type==4?1.f:b.type==5?.16f:b.type==6?1.2f:.65f;if(b.age>life)b.alive=false;}
  if(!extra.mode&&chapter>=3){frontier_tick(dt);}
  else if(!extra.mode&&chapter==1){int r=nearest(base_position(),relay,2);if(r>=0){bool near=false;for(auto& a:e)if(a.hp>0&&a.team==0&&land_combat(a.kind)&&dist(a.p,e[r].p)<3)near=true;if(near){capture+=dt;if(capture>=8){e[r].team=0;story_beat=true;mission.events|=1u<<5;say("苏弥：中继站已上线。坚持住，平民正在撤离！",8);}}}if(count(relay,0)){radio_time+=dt;if(radio_time>=90)won=true;}if(!count(relay,0)&&!count(relay,2))defeated=true;}
  else if(!extra.mode&&!count(hq,1)&&(chapter!=2||!count(power,1)))won=true;
  if(!count(hq,0))defeated=true;if(defeated)won=false;
  if(!extra.mode&&chapter==0&&!story_beat&&(authored_map()?seen[cell({21,19})]:time>50)){story_beat=true;mission.events|=1u<<6;say("苏弥：灰塔在用民用电网供能。我们必须查清真相。",8);}
  if(extra.mode&&!defeated&&wave>int(extra.cleared)&&army(1)==0)defense_clear();
  if(!extra.mode&&chapter==2&&!count(power,1)&&!story_beat){story_beat=true;mission.events|=1u<<7;for(auto& a:e)if(a.team==1&&defence_kind(a.kind))a.cool=9999;say("林岚：敌方电网瘫痪！全军向灰塔推进！",8);}
 }
#include "combined_arms.inc"
#include "squads.inc"
#include "tactical_orders.inc"
#include "production.inc"
#include "campaign_map.inc"
#include "frontier.inc"
};
}
