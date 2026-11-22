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
constexpr int N=64,MAX=100,CHAPTERS=8,squad_count=3;
constexpr int harbor_chapter=6,lighthouse_chapter=7;
struct Vec {float x=0,y=0;};
inline float distance_squared(Vec a,Vec b){float x=a.x-b.x,y=a.y-b.y;return x*x+y*y;}
inline float dist(Vec a,Vec b){return std::hypot(a.x-b.x,a.y-b.y);}
inline int cell(Vec p){return std::clamp(int(p.y),0,N-1)*N+std::clamp(int(p.x),0,N-1);}
inline Vec center(int c){return {float(c%N)+.5f,float(c/N)+.5f};}
enum Terrain:uint8_t {ground=0,woodland=1,water=2,cleared=3,crater=4,ruins=5,road=6,bridge=7,ridge=8,railbed=9};
enum Kind:uint8_t {rifle,rocket,tank,artillery,harvester,hq,power,refinery,barracks,factory,turret,relay,supply,patrol_boat,rotorcraft,commander,shipyard,helipad,aa_tower,landing_craft,kind_count};
// Keep published kind IDs stable. Domain and building checks are independent of ID order.
constexpr bool naval_kind(int k){return k==patrol_boat||k==landing_craft;}
constexpr bool combat_kind(int k){return (k>=rifle&&k<harvester)||naval_kind(k)||k==rotorcraft||k==commander;}
constexpr bool unit_kind(int k){return combat_kind(k)||k==harvester;}
constexpr bool land_combat(int k){return combat_kind(k)&&!naval_kind(k)&&k!=rotorcraft;}
constexpr bool construction_kind(int k){return (k>=power&&k<=turret)||(k>=shipyard&&k<=aa_tower);}
constexpr bool defence_kind(int k){return k==turret||k==aa_tower;}
constexpr int producer_for(int k){
 if(naval_kind(k))return shipyard;
 if(k==rotorcraft)return helipad;
 if(k==commander)return hq;
 return k<=rocket?barracks:factory;
}
constexpr bool production_building(int k){return k==hq||k==barracks||k==factory||k==shipyard||k==helipad;}
constexpr bool can_target(int attacker,int victim){
 if(attacker==landing_craft)return false;
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
 {"防空塔",260,610,0,7.4f,58,1.15f,6,-15},
 {"摆渡运输艇",340,470,1.85f,0,0,1,8,0}
};
static_assert(std::size(specs)==kind_count);
enum class Faction : uint8_t { northern, gray_tower };
constexpr uint32_t gray_tower_defense=2;
constexpr Faction faction_for(uint32_t mode,int team){
 int gray_team=mode==gray_tower_defense?0:1;
 return team==gray_team?Faction::gray_tower:Faction::northern;
}
inline constexpr Spec gray_tower_specs[]={
 {"铁卫步兵",100,110,1.08f,3.7f,14,.76f,3.5f,0},
 {"矢锋导弹兵",160,94,1.f,4.5f,36,1.85f,4.5f,0},
 {"壁垒重坦",385,490,1.6f,4.8f,44,1.6f,7.5f,0},
 {"震地火炮",470,250,1.35f,7.4f,80,3.3f,9,0},
 {"重型矿运车",290,440,1.62f,0,0,1,5.5f,0},
 {"灰塔指挥堡",0,2320,0,0,0,1,1,-10},
 {"熔炉电站",230,700,0,0,0,1,6,60},
 {"高炉精炼厂",370,940,0,0,0,1,7.5f,-15},
 {"铁卫兵营",225,740,0,0,0,1,5.5f,-10},
 {"壁垒工厂",530,1200,0,0,0,1,10,-30},
 {"堡垒炮台",275,685,0,5.5f,34,1.32f,6.5f,-15},
 specs[relay],specs[supply],
 {"黑潮巡逻艇",310,375,1.95f,5.5f,40,1.4f,7,0},
 {"隼式截击机",545,320,3.8f,6.2f,26,.95f,10,0},
 {"灰塔督导",550,360,1.4f,4.8f,34,.9f,11,0},
 {"灰塔船坞",375,890,0,0,0,1,8.5f,-15},
 {"截击机坪",460,910,0,0,0,1,9.5f,-25},
 {"天幕防空塔",290,650,0,7.8f,61,1.16f,7,-15},
 {"重型登陆艇",360,505,1.75f,0,0,1,9,0}
};
static_assert(std::size(gray_tower_specs)==kind_count);
constexpr const Spec& faction_spec(int kind,Faction faction){
 return faction==Faction::gray_tower?gray_tower_specs[kind]:specs[kind];
}
struct Entity {Vec p{},goal{},step{};float hp=0,cool=0,progress=0,dir=0,flash=0,stuck=0;int16_t target=-1;uint8_t kind=0,team=0,cargo=0,kills=0;bool selected=false,moving=false;};
constexpr int retreat_order=-3,escort_order_base=-4;
constexpr int board_order_base=escort_order_base-MAX,passenger_order_base=board_order_base-MAX;
// Enemy orders occupy six previously unused values beneath the passenger range.
enum class RaidRole : uint8_t { assault, flank, intercept, count };
constexpr int raid_order_base=passenger_order_base-MAX,raid_roles=int(RaidRole::count);
constexpr int last_raid_order=raid_order_base-2*raid_roles+1;
constexpr int raid_order(RaidRole role,bool staging){
 return raid_order_base-int(role)-(staging?raid_roles:0);
}
inline bool raider(const Entity& a){return a.target<=raid_order_base&&a.target>=last_raid_order;}
inline bool rallying(const Entity& a){return raider(a)&&a.target<=raid_order_base-raid_roles;}
inline RaidRole raid_role(const Entity& a){return RaidRole((raid_order_base-a.target)%raid_roles);}
constexpr int transport_capacity=4;
constexpr float boarding_range=2.25f;
inline int bound_order_slot(const Entity& a,int base){
 int id=base-a.target;return id>=0&&id<MAX?id:-1;
}
inline int escort_id(const Entity& a){return bound_order_slot(a,escort_order_base);}
inline int boarding_ship(const Entity& a){return bound_order_slot(a,board_order_base);}
inline int passenger_ship(const Entity& a){return bound_order_slot(a,passenger_order_base);}
inline bool deployed(const Entity& a){return a.hp>0&&passenger_ship(a)<0;}
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
enum HarborRoute : uint8_t { harbor_choice, harbor_beacon, harbor_shutdown };
struct Mission {uint32_t events=0,read_events=0;uint8_t stage=0,delivered=0;std::array<uint8_t,3> waypoint{};HarborRoute harbor_plan=harbor_choice;std::array<int16_t,3> convoy{{-1,-1,-1}};float support_left=0;};
constexpr Vec harbor_beacon_site{30.5f,25.5f},harbor_switch_site{16.5f,37.5f};
constexpr float harbor_hold_seconds=45;
constexpr unsigned harbor_beacon_radio=8,harbor_shutdown_radio=9;
enum LighthouseStage : uint8_t { restore_grid, capture_lighthouse, guide_ferries };
constexpr Vec island_grid_site{42.5f,33.5f},lighthouse_site{54.5f,16.5f};
constexpr float lighthouse_hold_seconds=35;
constexpr unsigned island_grid_radio=10,lighthouse_radio=11;
enum class GroundMaterial : uint8_t { grass, red_gravel, marsh, scree, asphalt };
enum class DefenseMap : uint8_t { river, reef, ridge };
struct DefenseArena {
 const char* title;
 const char* terrain_note;
 int side, credits, first_wave;
 GroundMaterial material;
 Vec view, resupply;
 std::array<Vec,3> entries;
 const char* orders;
};
inline constexpr DefenseArena defense_arenas[]={
 {"河谷前哨","三处渡口",28,1500,35,GroundMaterial::grass,{8,10},{15,16},
  {{{23,5},{24,22},{5,24}}},"把坦克放在渡口附近，留一队守矿车。"},
 {"礁湾海防","堤道与舰艇",48,1850,44,GroundMaterial::marsh,{35,24},{37,33},
  {{{7,9},{6,22},{8,39}}},"守住西边两条堤道，也给船坞留一支巡逻队。"},
 {"石岭矿场","山路与炮兵",44,1700,42,GroundMaterial::scree,{23,34},{28,33},
  {{{6,7},{22,5},{38,10}}},"山脊把来路分开。看清敌军从哪条山路过来，再布置火炮。"},
};
constexpr unsigned defense_map_count=std::size(defense_arenas);
struct CampaignMap {
 int side, credits, first_wave;
 GroundMaterial material;
 Vec home, front;
 std::array<Vec, 2> entries;
 const char* arrival;
};
inline constexpr CampaignMap campaign_maps[] = {
 {36, 1300, 84, GroundMaterial::red_gravel, {7,25}, {11,22}, {{{31,7},{31,25}}}, "林岚：矿车开始干活了。沿运矿路走，灰塔的指挥堡在东北。"},
 {44, 1500, 105, GroundMaterial::marsh, {36,24}, {32,24}, {{{5,8},{4,34}}}, "苏弥：居民在西岸。我们先过河，去占领中继站。"},
 {48, 1600, 115, GroundMaterial::asphalt, {23,7}, {23,12}, {{{6,35},{42,39}}}, "林岚：东西两边各有一座军用电站。先打电站，再去南边。"},
 {54, 1750, 125, GroundMaterial::scree, {9,40}, {14,37}, {{{48,10},{48,42}}}, "苏弥：先去占领北边山口和东边矿场的电台。我需要里面的记录。"},
 {60, 1900, 135, GroundMaterial::grass, {51,10}, {47,16}, {{{16,12},{22,47}}}, "何舟：卡车在东北货场等着。先占领货场电台，我们才能出发。"},
 {64, 2100, 150, GroundMaterial::asphalt, {53,53}, {52,48}, {{{7,12},{48,7}}}, "林岚：沿东南大道进城。先占领三座调度站，保住医院用电。"},
 {64, 1900, 105, GroundMaterial::marsh, {44,9}, {43,16}, {{{9,49},{18,54}}}, "苏弥：可以搭船去岛上的信标，也可以沿南边的堤道去停机站。"},
 {64, 2150, 135, GroundMaterial::scree, {16,43}, {21,44}, {{{55,36},{56,9}}}, "苏弥：白石岛的灯没亮。先去岛上的配电站，船在礁区外等着。"},
};
static_assert(std::size(campaign_maps)==CHAPTERS);
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
 Faction faction(int team=0)const{return faction_for(extra.mode,team);}
 DefenseMap defense_map()const{return extra.mode&&extra.map_rules?DefenseMap(extra.map_rules-1):DefenseMap::river;}
 const DefenseArena& arena()const{return defense_arenas[unsigned(defense_map())];}
 const Spec& spec(int kind,int team=0)const{return faction_spec(kind,faction(team));}
 const Spec& spec(const Entity& a)const{return spec(a.kind,a.team);}
 int count(int k,int team=0)const{int n=0;for(auto& a:e)n+=a.hp>0&&a.team==team&&a.kind==k;return n;}
 int army(int team=0)const{int n=0;for(auto& a:e)n+=a.hp>0&&a.team==team&&combat_kind(a.kind);return n;}
 bool open(Vec p,int ignore=-1)const{if(p.x<1||p.y<1||p.x>=side-1||p.y>=side-1||(terrain[cell(p)]==water||terrain[cell(p)]==ridge))return false;for(int i=0;i<MAX;++i){auto& a=e[i];if(i!=ignore&&a.hp>0&&building(a.kind)&&distance_squared(a.p,p)<(defence_kind(a.kind)?.4225f:1.44f))return false;}return true;}
 int add(int kind,int team,Vec p){for(int i=0;i<MAX;++i)if(e[i].hp<=0){squads[i]=0;e[i]={};auto& a=e[i];a.p=a.goal=a.step=p;a.hp=spec(kind,team).hp;a.kind=kind;a.team=team;if(combat_kind(kind)||defence_kind(kind))a.cargo=uint8_t(random()%255+1);a.dir=team?3.14f:0;return i;}return -1;}
 void init(int ch,HarborRoute plan=harbor_choice) {
  new(this) World{};
  chapter = std::clamp(ch, 0, CHAPTERS-1);
  if(chapter==lighthouse_chapter)mission.harbor_plan=plan;
  init_campaign();
 }
 void init_defense(uint32_t seed,Faction player=Faction::northern,DefenseMap map=DefenseMap::river){
  new(this) World{};
  extra.mode=player==Faction::gray_tower?gray_tower_defense:1;
  extra.map_rules=uint8_t(1+unsigned(map));
  extra.seed=seed?seed:83711;
  side=arena().side;
  credits=arena().credits;
  next_wave=arena().first_wave;
  terrain.fill(water);
  switch(map){
   case DefenseMap::river:init_river_defense();break;
   case DefenseMap::reef:init_reef_defense();break;
   case DefenseMap::ridge:init_ridge_defense();break;
  }
  say(arena().orders,8);
  power_update();reveal();
 }
 bool paved(Vec p)const{return terrain[cell(p)]==road||terrain[cell(p)]==bridge;}
 bool in_cover(const Entity& a)const{return extra.map_rules&&(a.kind<=rocket||a.kind==commander)&&(terrain[cell(a.p)]==woodland||terrain[cell(a.p)]==crater||terrain[cell(a.p)]==ruins);}
 float move_speed(const Entity& a)const{float speed=spec(a).speed;if(a.kind==rotorcraft||naval_kind(a.kind))return speed;bool vehicle=a.kind>=tank&&a.kind<=harvester;if(a.kind==harvester&&a.cargo)speed=1.42f;if(paved(a.p))speed*=vehicle?1.2f:1.05f;else if(terrain[cell(a.p)]==woodland)speed*=vehicle?.82f:.95f;else if(terrain[cell(a.p)]==crater||terrain[cell(a.p)]==ruins)speed*=vehicle?.78f:.9f;return speed;}
 Vec wave_entry(int next) const {
  if(!extra.mode&&chapter==harbor_chapter&&mission.stage==harbor_beacon)
   return next%2?Vec{27.5f,9.5f}:Vec{29.5f,47.5f};
  if (authored_map()) return campaign_maps[chapter].entries[(next-1)&1];
  if (extra.mode) return arena().entries[(extra.seed+unsigned(next))%3];
  if (chapter>=3) return next%2?Vec{float(side-5),6}:Vec{float(side-5),float(side-6)};
  return chapter==1?Vec{23,24}:Vec{24,22};
 }
 const char* wave_direction(int next) const {
  if(naval_raid(next))return "陆路与水路";
  if(dual_raid_entry(next))return "双路";
  auto p=wave_entry(next);
  if (!authored_map()&&defense_map()==DefenseMap::river) return p.x<10?"南路":p.y<10?"东北":"东南";
  int east = p.x > side*2.f/3, west = p.x < side/3.f;
  if (p.y < side/3.f) return west?"西北":east?"东北":"北面";
  if (p.y > side*2.f/3) return west?"西南":east?"东南":"南面";
  return west?"西面":east?"东面":"中路";
 }
 GroundMaterial material() const {
  return extra.mode?arena().material:authored_map()?campaign_maps[chapter].material:GroundMaterial::grass;
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
  int base=nearest({6,9},hq,0);if(base>=0)for(auto& a:e)if(a.hp>0&&a.team==0&&unit_kind(a.kind)&&dist(a.p,e[base].p)<8){if(a.hp<spec(a).hp)a.hp=std::min(float(spec(a).hp),a.hp+spec(a).hp*.12f);blast(a.p,3);}
  if(wave%3==0){
   Vec station=defense_map()==DefenseMap::river&&wave%2==0?Vec{18,8}:arena().resupply;
   if(open(station)&&add(supply,2,station)>=0)power_update();
  }
  char text[120];std::snprintf(text,sizeof text,"第 %d 波已击退！补给 +%d，前哨维修完成",wave,reward);say(text,8);emit_sound(5);
 }
 void power_update(){
  for(int c=0;c<N*N;++c)obstacles[c]=terrain[c]==water||terrain[c]==ridge;
  power_net=0;
  for(const auto& a:e){
   if(a.hp<=0||!building(a.kind))continue;
   if(a.team==0&&a.progress<=0)power_net+=spec(a).supply;
   int x=int(a.p.x),y=int(a.p.y);
   float limit=defence_kind(a.kind)?.4225f:1.44f;
   for(int yy=std::max(0,y-1);yy<=std::min(N-1,y+1);++yy)
    for(int xx=std::max(0,x-1);xx<=std::min(N-1,x+1);++xx){
     int c=yy*N+xx;
     if(distance_squared(a.p,center(c))<limit)obstacles[c]=1;
    }
  }
 }
 int ready_count(int k,int team=0)const{int n=0;for(auto& a:e)n+=a.hp>0&&a.team==team&&a.kind==k&&a.progress<=0;return n;}
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
  if(credits<spec(k).cost)return "矿石不足，等待采矿车返航";
  if(job_count>=6)return "生产队列已满";
  int roster=army();
  for(int i=0;i<job_count;++i)roster+=combat_kind(jobs[i].kind);
  if(roster>=36&&combat_kind(k))return "部队已达 36 人上限";
  return nullptr;
 }
 void select_all(){for(auto& a:e)a.selected=deployed(a)&&a.team==0&&combat_kind(a.kind);say("已选择全部作战部队");}
 int selected()const{int n=0;for(auto& a:e)n+=a.hp>0&&a.selected;return n;}
 void select_one(int id,bool append=false){for(int i=0;i<MAX;++i)if(!append)e[i].selected=false;if(id>=0&&id<MAX&&deployed(e[id])&&e[id].team==0)e[id].selected=true;}
 int hit(Vec p,float radius=1.f)const{int found=-1;float best=radius;for(int i=0;i<MAX;++i){auto& a=e[i];if(!deployed(a)||(!fog[cell(a.p)]&&a.team!=0))continue;float d=dist(p,a.p);if(d<best){best=d;found=i;}}return found;}
 void stop_selected(){int n=0;for(auto& a:e)if(deployed(a)&&a.selected&&a.team==0&&combat_kind(a.kind)){a.goal=a.step=a.p;a.moving=false;a.target=-1;++n;}say(n?"编队就地警戒，自动还击":"请先选择作战部队");}
 Vec formation_goal(Vec p,int index,int kind=rifle)const{
  float offset_x=((index%4)-1.5f)*.65f,offset_y=(index/4)*.65f;
  Vec goal{std::clamp(p.x+offset_x,1.2f,side-1.5f),std::clamp(p.y+offset_y,1.2f,side-1.5f)};
  return open_for(kind,goal)?goal:p;
 }
 void order(Vec p,int target=-1){
  if(target>=MAX)target=-1;
  int n=0,rallies=0;
  for(auto& a:e){
   if(!deployed(a)||!a.selected||a.team!=0)continue;
   if(production_building(a.kind)){
    int kind=a.kind==shipyard?patrol_boat:a.kind==helipad?rotorcraft:rifle;
    if(target<0&&open_for(kind,p)){a.goal=p;a.moving=true;++rallies;}
    continue;
   }
   if(!unit_kind(a.kind))continue;
   if(target<0&&naval_kind(a.kind)&&!open_for(a.kind,p))continue;
   if(target>=0&&!can_target(a.kind,e[target].kind))continue;
   a.goal=formation_goal(p,n++,a.kind);a.step=a.p;a.stuck=0;a.moving=true;a.target=target;
  }
  if(n){say(target>=0?"集中火力，攻击目标！":"收到，编队前进。");emit_sound(1);}
  else if(rallies){say("集结点已设置，新部队会自动前往");emit_sound(1);}
  else if(selected())say("检查目标和可通行的地形");
 }
 [[gnu::noinline]] void reveal(){
  fog.fill(0);
  for(const auto& a:e)if(deployed(a)&&a.team==0){
   int radius=a.kind==rotorcraft?9:a.kind==hq?7:lighthouse_landmark(a)==1?11:6;
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
  bool naval=naval_kind(kind);
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

 [[gnu::noinline]] void move(int id,float dt){auto& a=e[id];if(dist(a.p,a.goal)<.18f){a.moving=false;return;}if(a.kind==rotorcraft)a.step=a.goal;else if(dist(a.p,a.step)<.13f||!open_for(a.kind,a.step,id)){if(!route_credits)return;--route_credits;a.step=path(a.p,a.goal,id);}float d=dist(a.p,a.step);if(d<.02f){a.stuck+=dt;if(a.stuck>2)a.moving=false;return;}a.stuck=0;float heading=std::atan2(a.step.y-a.p.y,a.step.x-a.p.x),alignment=1;if((a.kind>=tank&&a.kind<=harvester)||naval_kind(a.kind)||a.kind==rotorcraft){float difference=std::remainder(heading-a.dir,6.2831853f),turn=(a.kind==rotorcraft?8.f:naval_kind(a.kind)?5.f:a.kind==artillery?2.6f:a.kind==harvester?4.2f:3.5f)*dt;a.dir=std::remainder(a.dir+std::clamp(difference,-turn,turn),6.2831853f);alignment=std::max(0.f,std::cos(std::remainder(heading-a.dir,6.2831853f)));}else a.dir=heading;float v=std::min(d,move_speed(a)*alignment*dt);a.p.x+=(a.step.x-a.p.x)/d*v;a.p.y+=(a.step.y-a.p.y)/d*v;
  // Soft separation makes formations readable without deadlocking a crossing.
  for(int j=0;j<id;++j)if(deployed(e[j])&&unit_kind(e[j].kind)&&naval_kind(e[j].kind)==naval_kind(a.kind)&&(e[j].kind==rotorcraft)==(a.kind==rotorcraft)){float d2=distance_squared(a.p,e[j].p);if(d2>.01f*.01f&&d2<.38f*.38f){float dd=std::sqrt(d2);Vec n={a.p.x+(a.p.x-e[j].p.x)/dd*dt*.25f,a.p.y+(a.p.y-e[j].p.y)/dd*dt*.25f};if(open_for(a.kind,n,id))a.p=n;}}
 }
 void blast(Vec p,int type=0){for(auto& b:blasts)if(!b.alive){b={p,0,uint8_t(type),true};return;}}
 bool shielded(const Entity& a)const{
  return !extra.mode&&a.team==1&&a.kind==hq&&
   ((chapter==5&&mission.stage<2)||(chapter==harbor_chapter&&mission.stage!=harbor_shutdown));
 }
 void damage(int id,float value,int source=-1,int weapon=-1){
  if(id<0||id>=MAX||!deployed(e[id]))return;auto& a=e[id];if(shielded(a))return;if(a.team==0)value*=1-extra.armor*.10f;int attack_kind=weapon>=0?weapon:source>=0&&source<MAX?int(e[source].kind):-1;if(in_cover(a)&&attack_kind>=0&&attack_kind!=artillery)value*=.75f;a.hp-=value;a.flash=.14f;
  if(a.hp>0)return;
  a.hp=0;if(a.kind==landing_craft)remove_transport_passengers(id);
  if(a.kind==harvester&&a.team!=1)finish_escort_target(id);
  blast(a.p,building(a.kind)?2:1);emit_sound(building(a.kind)?12:a.kind>=tank?13:3);
  int c=cell(a.p);
  bool was_ridge=terrain[c]==ridge;
  if(a.kind!=rotorcraft&&terrain[c]!=water&&terrain[c]!=bridge&&terrain[c]!=railbed&&!ore[c])terrain[c]=building(a.kind)?ruins:crater;
  squads[id]=0;
  if(a.team==1){++killed;credits+=building(a.kind)?100:25;}else if(a.team==0){++lost;if(production_building(a.kind))refund_destroyed_facility(id);}
  if(source>=0&&source<MAX&&e[source].hp>0&&e[source].team!=a.team&&e[source].team!=2&&a.team!=2){
   auto& killer=e[source];unsigned before=veteran_rank(killer);killer.kills=std::min(255,int(killer.kills)+1);
   if(veteran_rank(killer)>before){if(killer.hp<spec(killer).hp)killer.hp=std::min(float(spec(killer).hp),killer.hp+spec(killer).hp*.2f);blast(killer.p,4);if(killer.team==0){say("部队晋升！火力增强，战地修复已完成");emit_sound(4);}}
  }
  if(building(a.kind)||(was_ridge&&terrain[c]!=ridge))power_update();
 }
 float attack_damage(const Entity& attacker,const Entity& victim)const{
  float value=spec(attacker).damage;
  if(attacker.kind==rotorcraft&&victim.kind==rotorcraft&&faction(attacker.team)==Faction::gray_tower)value*=2.2f;
  return value;
 }
 void fire(int id,int target){auto& a=e[id];if(!deployed(e[target])||!can_target(a.kind,e[target].kind))return;bool admitted=false;
  for(auto& s:shots)if(!s.alive){s={};s.from=a.p;s.p=a.p;s.dest=e[target].p;s.target=int16_t(target|(unsigned(a.cargo)<<7));s.team=uint8_t((id+1)*2+a.team);s.kind=a.kind;s.damage=attack_damage(a,e[target])*(1+veteran_rank(a)*.12f)*(a.team==0?1+extra.damage*.12f:1);s.duration=a.kind==artillery?.85f:(a.kind==rocket||a.kind==aa_tower||a.kind==rotorcraft)?.5f:.22f;s.alive=true;admitted=true;break;}
  if(!admitted)return;a.cool=spec(a).reload;a.dir=std::atan2(e[target].p.y-a.p.y,e[target].p.x-a.p.x);unsigned cue=(a.kind==rifle||a.kind==commander)?6:(a.kind==rocket||a.kind==aa_tower||a.kind==rotorcraft)?7:a.kind==artillery?9:8;if(sound<=2)sound=cue;else if(sound>=6&&sound<=9)sound=std::max(sound,cue);
 }
 void supply_tick(Entity& station,float dt){
  bool friendly=false,contested=false;for(auto& a:e)if(deployed(a)&&land_combat(a.kind)&&dist(a.p,station.p)<2.7f){friendly|=a.team==0;contested|=a.team==1;}
  if(!friendly){station.progress=std::max(0.f,station.progress-dt*.5f);return;}
  if(contested)return;station.progress+=dt;if(station.progress<5)return;
  credits+=280;for(auto& a:e)if(deployed(a)&&a.team==0&&combat_kind(a.kind)&&dist(a.p,station.p)<4){if(a.hp<spec(a).hp)a.hp=std::min(float(spec(a).hp),a.hp+spec(a).hp*.3f);blast(a.p,3);}
  station.hp=0;terrain[cell(station.p)]=3;blast(station.p,6);power_update();say("补给接收！矿石 +280，附近部队已维修",7);emit_sound(5);
 }
 bool strike(Vec p){if(strike_cd>0){say("支援还在冷却中");return false;}if(credits<180){say("空袭需要 180 矿石");return false;}if(!fog[cell(p)]){say("空袭需要可见目标");return false;}credits-=180;strike_cd=45;for(int i=0;i<MAX;++i)if(e[i].hp>0&&e[i].team==1&&e[i].kind!=rotorcraft&&dist(e[i].p,p)<3.8f)damage(i,260);for(int i=0;i<7;++i)blast({p.x+(int(random()%400)-200)*.01f,p.y+(int(random()%400)-200)*.01f},2);say("空中支援：雷鸣打击已抵达！");emit_sound(3);return true;}
 void repair(){for(auto& a:e)if(a.selected&&a.hp>0&&a.team==0){int cost=int((spec(a).hp-a.hp)*.2f);if(cost<=0)continue;if(credits<cost){say("修复资金不足");return;}credits-=cost;a.hp=spec(a).hp;blast(a.p,3);}say("维修完成");}
 [[gnu::noinline]] void tick(float dt){if(won||defeated)return;dt=std::clamp(dt,0.f,.1f);time+=dt;route_credits=6;autosave+=dt;notice_time=std::max(0.f,notice_time-dt);strike_cd=std::max(0.f,strike_cd-dt);reveal_timer-=dt;
  if(std::floor(time*2)!=std::floor((time-dt)*2))enemy_orders();
  if(reveal_timer<=0){reveal();reveal_timer=.5f;}if(extra.mode){if(wave==int(extra.cleared)&&army(1)==0&&time>=next_wave)spawn_raid();}else if(time>=next_wave&&count(hq,1)){
   if(chapter==harbor_chapter&&mission.stage==harbor_beacon)harbor_raid();
   else spawn_raid();
  }research_tick(dt);
  mission.support_left=std::max(0.f,mission.support_left-dt);
  construction_tick(dt);training_tick(dt);
  for(int i=0;i<MAX;++i){auto& a=e[i];if(!deployed(a))continue;a.flash=std::max(0.f,a.flash-dt);a.cool=std::max(0.f,a.cool-dt);
   if(a.kind==supply){supply_tick(a,dt);continue;}if(chapter==4&&a.team==2&&a.kind==harvester)continue;
   if(a.progress>0){a.progress-=dt*(power_net<0?.5f:1.f);if(a.progress<=0){a.progress=0;say("工程完成");if(a.kind==refinery){add(harvester,0,{a.p.x+1.5f,a.p.y+1.5f});}power_update();if(sound<3||(sound>=6&&sound<=9))emit_sound(11);}continue;}
   if(a.kind==harvester&&a.team==0){harvest(i,dt);continue;}
   if(building(a.kind)&&!defence_kind(a.kind))continue;
   if(a.team==0&&combat_kind(a.kind)){
    if(boarding_ship(a)>=0){update_boarding(i,dt);continue;}
    if(a.target==retreat_order){
     if(a.moving)move(i,dt);
     if(!a.moving)a.target=-1;
     continue;
    }
    if(escort_id(a)>=0)update_escort(a,dt);
   }
   if(a.kind==landing_craft){if(a.moving)move(i,dt);continue;}
   int target=-1;float best=spec(a).range;
   if(a.target>=0&&a.target<MAX&&deployed(e[a.target])&&e[a.target].team!=a.team&&e[a.target].team!=2&&can_target(a.kind,e[a.target].kind)){if(dist(a.p,e[a.target].p)<best+(building(e[a.target].kind)?1:0))target=a.target;else if(unit_kind(a.kind)){a.goal=e[a.target].p;a.moving=true;}}
   else if(escort_id(a)<0&&!raider(a))a.target=-1;
   if(target<0){
    bool interceptor=a.kind==rotorcraft&&faction(a.team)==Faction::gray_tower,tracking_air=false;
    for(int j=0;j<MAX;++j){
     auto& b=e[j];
     if(!deployed(b)||b.team==a.team||!can_target(a.kind,b.kind)||(b.team==2&&(a.team!=1||chapter!=4||b.kind!=harvester))||(!fog[cell(b.p)]&&a.team==0))continue;
     bool air_priority=interceptor&&b.kind==rotorcraft;
     if(tracking_air&&!air_priority)continue;
     float pad=building(b.kind)?.6f:0,range=(air_priority&&!tracking_air?spec(a).range:best)+pad;
     float d2=distance_squared(a.p,b.p);
     if(range>0&&d2<range*range){target=j;best=std::sqrt(d2)-pad;tracking_air=air_priority;}
    }
   }
   bool powered=!defence_kind(a.kind)||(a.team==1?enemy_defences_powered():power_net>=0);
   if(target>=0&&powered){if(a.cool<=0)fire(i,target);}else if(a.moving&&unit_kind(a.kind))move(i,dt);
  }
  sync_passengers();
  for(auto& s:shots)if(s.alive){s.t+=dt;float f=std::min(1.f,s.t/s.duration);s.p={s.from.x+(s.dest.x-s.from.x)*f,s.from.y+(s.dest.y-s.from.y)*f};if(f>=1){
   s.alive=false;float value=s.damage;int victim_id=shot_target(s),source=shot_source(s);
   if(source<0||source>=MAX||e[source].hp<=0||e[source].team!=shot_team(s)||e[source].kind!=s.kind||e[source].cargo!=(unsigned(s.target)>>7))source=-1;
   if(victim_id>=0&&victim_id<MAX){auto& victim=e[victim_id];if(can_target(s.kind,victim.kind)&&dist(victim.p,s.dest)<1.8f){if(s.kind==rifle&&victim.kind>=tank&&victim.kind!=commander)value*=.32f;if(s.kind==rocket&&(victim.kind==rifle||victim.kind==commander))value*=.55f;damage(victim_id,value,source,s.kind);}}
   blast(s.dest,0);if(s.kind==artillery)for(int j=0;j<MAX;++j)if(j!=victim_id&&e[j].hp>0&&e[j].team!=shot_team(s)&&e[j].team!=2&&e[j].kind!=rotorcraft&&dist(e[j].p,s.dest)<1.7f)damage(j,value*.5f,source,artillery);
  }}
  for(auto& b:blasts)if(b.alive){b.age+=dt;float life=b.type==2?2.2f:b.type==1?1.1f:b.type==3?.7f:b.type==4?1.f:b.type==5?.16f:b.type==6?1.2f:.65f;if(b.age>life)b.alive=false;}
  if(!extra.mode&&chapter==lighthouse_chapter){lighthouse_tick(dt);}
  else if(!extra.mode&&chapter==harbor_chapter){harbor_tick(dt);}
  else if(!extra.mode&&chapter>=3){frontier_tick(dt);}
  else if(!extra.mode&&chapter==1){int r=nearest(base_position(),relay,2);if(r>=0){bool near=false;for(auto& a:e)if(deployed(a)&&a.team==0&&land_combat(a.kind)&&dist(a.p,e[r].p)<3)near=true;if(near){capture+=dt;if(capture>=8){e[r].team=0;story_beat=true;mission.events|=1u<<5;say("苏弥：中继站已上线。坚持住，平民正在撤离！",8);}}}if(count(relay,0)){radio_time+=dt;if(radio_time>=90)won=true;}if(!count(relay,0)&&!count(relay,2))defeated=true;}
  else if(!extra.mode&&!count(hq,1)&&(chapter!=2||!count(power,1)))won=true;
  if(!count(hq,0))defeated=true;if(defeated)won=false;
  if(!extra.mode&&chapter==0&&!story_beat&&(authored_map()?seen[cell({21,19})]:time>50)){story_beat=true;mission.events|=1u<<6;say("苏弥：灰塔在用民用电网供能。我们必须查清真相。",8);}
  if(extra.mode&&!defeated&&wave>int(extra.cleared)&&army(1)==0)defense_clear();
  if(!extra.mode&&chapter==2&&!count(power,1)&&!story_beat){story_beat=true;mission.events|=1u<<7;for(auto& a:e)if(a.team==1&&defence_kind(a.kind))a.cool=9999;say("林岚：敌方电网瘫痪！全军向灰塔推进！",8);}
 }
#include "combined_arms.inc"
#include "transport.inc"
#include "squads.inc"
#include "tactical_orders.inc"
#include "production.inc"
#include "campaign_map.inc"
#include "defense_maps.inc"
#include "frontier.inc"
#include "harbor.inc"
#include "lighthouse.inc"
#include "enemy_ai.inc"
};
}
