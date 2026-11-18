// SPDX-License-Identifier: MIT
#pragma once
#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>
namespace garden {
constexpr int rows=5,columns=9,plant_count=7,stage_count=12;
enum class Defender:uint8_t {none,trophy,cactus,stone,gnome,frost,ember,clover};
enum class EnemyKind:uint8_t {dust,beetle,snail,moth};
enum class ShotKind:uint8_t {seed,leaf,frost,ember};
constexpr int cost[]={0,40,10,30,20,35,50,30};
constexpr int health[]={0,100,100,260,100,95,110,85};
struct Stage {float duration,interval;unsigned enemies;int stars;};
inline constexpr std::array<Stage,stage_count> stages={{{75,5.7f,1,110},{85,5.4f,3,120},{90,5.1f,3,130},{95,5.0f,7,150},{100,4.9f,7,160},{100,4.8f,15,170},{105,4.7f,15,180},{110,4.5f,15,190},{110,4.3f,15,200},{115,4.1f,15,210},{120,3.9f,15,220},{120,3.7f,15,240}}};
struct Plant {Defender kind=Defender::none;float hp=0,cycle=0,animation=0,flash=0;bool attacking=false;};
struct Enemy {bool active=false,fox=false,attacking=false;int row=0,jumped=-1;float x=0,hp=50,cycle=0,animation=0,jump=0,flash=0,slow=0;EnemyKind kind=EnemyKind::dust;};
struct Shot {bool active=false,axe=false;int row=0;float x=0;ShotKind kind=ShotKind::seed;};
struct Effect {bool active=false,enemy=false,beetle=false;Defender kind=Defender::none;int row=0;float x=0,time=0;EnemyKind enemy_kind=EnemyKind::dust;};
enum class Outcome {playing,won,lost};
struct Game {
 std::array<Plant,rows*columns> plants{};std::array<Enemy,48> enemies{};std::array<Shot,96> shots{};std::array<Effect,24> effects{};
 uint32_t seed=0x41c6ce57;int stars=110,kills=0,level=0,generated=0,shots_fired=0,waves=0;float elapsed=0,next_spawn=8,dew_tick=0;Outcome outcome=Outcome::playing;
 uint32_t random(){seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;return seed;}
 void start(int stage,uint32_t value=0x41c6ce57){*this={};level=std::clamp(stage,0,stage_count-1);seed=value?value:1;stars=stages[level].stars;}
 float duration()const{return stages[level].duration;}
 bool place(int r,int c,Defender kind){if(outcome!=Outcome::playing||r<0||r>=rows||c<0||c>=columns||kind==Defender::none||unsigned(kind)>plant_count)return false;auto& p=plants[r*columns+c];if(p.kind!=Defender::none||stars<cost[int(kind)])return false;p={kind,float(health[int(kind)]),0,0,0,false};stars-=cost[int(kind)];return true;}
 bool remove(int r,int c){if(r<0||r>=rows||c<0||c>=columns||outcome!=Outcome::playing)return false;auto& p=plants[r*columns+c];if(p.kind==Defender::none)return false;p={};return true;}
 bool spawn(int r,EnemyKind kind){if(r<0||r>=rows||unsigned(kind)>3)return false;for(auto& e:enemies)if(!e.active){e={};e.active=true;e.row=r;e.x=9.3f;e.kind=kind;e.fox=kind==EnemyKind::beetle;e.hp=(kind==EnemyKind::snail?160:kind==EnemyKind::moth?38:55)*(1+.035f*level);return true;}return false;}
 bool spawn(int r,bool fox){return spawn(r,fox?EnemyKind::beetle:EnemyKind::dust);}
 void fire(int r,int c,ShotKind kind){for(auto& s:shots)if(!s.active){s={true,kind==ShotKind::leaf,r,c+.65f,kind};++shots_fired;return;}}
 void defeat(int r,float x,Defender kind,bool enemy=false,EnemyKind monster=EnemyKind::dust){for(auto& e:effects)if(!e.active){e={true,enemy,monster==EnemyKind::beetle,kind,r,x,0,monster};return;}}
 void damage(Enemy& e,float amount){e.hp-=amount;e.flash=.12f;if(e.hp<=0){defeat(e.row,e.x,Defender::none,true,e.kind);e.active=false;++kills;stars+=3;}}
 void step(float dt){
  if(outcome!=Outcome::playing||!(dt>0)||!std::isfinite(dt))return;dt=std::min(dt,.1f);elapsed+=dt;
  for(auto& e:effects)if(e.active){e.time+=dt;if(e.time>=.6f)e.active=false;}
  if(elapsed>=duration()){outcome=Outcome::won;return;}
  dew_tick+=dt;if(dew_tick>=2.5f){dew_tick-=2.5f;++stars;}
  if(elapsed>=next_spawn){unsigned allowed=stages[level].enemies;unsigned pick=random()%4;while(!(allowed&(1u<<pick)))pick=(pick+1)%4;spawn(int(random()%rows),EnemyKind(pick));++waves;
   if(level>=3&&waves%5==0)spawn(int(random()%rows),EnemyKind((allowed&8)?3:0));
   next_spawn=elapsed+std::max(2.1f,stages[level].interval-elapsed*.012f);}
  // Stable per-lane indices retain collision order while avoiding scans of
  // unrelated lanes for every plant and projectile. Deaths are checked live.
  std::array<std::array<uint8_t,48>,rows> lanes{};std::array<unsigned,rows> lane_count{};
  for(unsigned i=0;i<enemies.size();++i)if(enemies[i].active)lanes[enemies[i].row][lane_count[enemies[i].row]++]=i;
  for(int r=0;r<rows;++r)for(int c=0;c<columns;++c){auto& p=plants[r*columns+c];if(p.kind==Defender::none)continue;p.animation+=dt;p.flash=std::max(0.f,p.flash-dt);
   if(p.kind==Defender::trophy){p.cycle+=dt;if(p.cycle>=8.f){p.cycle-=8.f;stars+=10;++generated;p.flash=.35f;}continue;}
   if(p.kind==Defender::clover){p.cycle+=dt;p.attacking=p.cycle>3.2f;if(p.cycle>=4.f){p.cycle-=4.f;for(int rr=std::max(0,r-1);rr<=std::min(rows-1,r+1);++rr)for(int cc=std::max(0,c-1);cc<=std::min(columns-1,c+1);++cc){auto& friend_=plants[rr*columns+cc];if(friend_.kind!=Defender::none)friend_.hp=std::min(float(health[int(friend_.kind)]),friend_.hp+14.f);}}continue;}
   if(p.kind==Defender::stone){p.attacking=false;p.cycle+=dt;continue;}
   bool target=false;for(unsigned n=0;n<lane_count[r];++n){const auto& e=enemies[lanes[r][n]];if(e.active&&e.x>=c+.3f){target=true;break;}}if(target&&!p.attacking)p.cycle=p.animation=0;p.attacking=target;
   if(target){float period=p.kind==Defender::cactus?1.7f:p.kind==Defender::gnome?.9f:p.kind==Defender::frost?2.4f:3.8f;float before=p.cycle;p.cycle+=dt;if(before<.25f&&p.cycle>=.25f)fire(r,c,p.kind==Defender::cactus?ShotKind::seed:p.kind==Defender::gnome?ShotKind::leaf:p.kind==Defender::frost?ShotKind::frost:ShotKind::ember);if(p.cycle>=period)p.cycle-=period;}else p.cycle=0;
  }
  for(auto& s:shots)if(s.active){float previous=s.x;s.x+=dt*(s.kind==ShotKind::leaf?2.2f:3.f);Enemy* hit=nullptr;for(unsigned n=0;n<lane_count[s.row];++n){auto& e=enemies[lanes[s.row][n]];if(e.active&&e.x>=previous-.3f&&e.x<=s.x+.3f&&(!hit||e.x<hit->x))hit=&e;}
   if(hit){if(s.kind==ShotKind::ember){int r=hit->row;float x=hit->x;for(auto& e:enemies)if(e.active&&std::abs(e.row-r)<=1&&std::abs(e.x-x)<.9f)damage(e,38);}
    else{if(s.kind==ShotKind::frost)hit->slow=3.f;damage(*hit,s.kind==ShotKind::leaf?24:s.kind==ShotKind::frost?12:16);}s.active=false;}if(s.x>10)s.active=false;
  }
  for(auto& e:enemies)if(e.active){e.animation+=dt;e.flash=std::max(0.f,e.flash-dt);e.slow=std::max(0.f,e.slow-dt);float rate=e.slow>0?.45f:1.f;
   if(e.jump>0){e.jump-=dt;e.x-=3*dt*rate;e.attacking=false;continue;}
   Plant* target=nullptr;int cell=-1;for(int c=columns-1;c>=0;--c){auto& p=plants[e.row*columns+c];if(p.kind!=Defender::none&&e.x>=c+.18f&&e.x<=c+.85f){target=&p;cell=c;break;}}
   if(target&&e.fox&&target->kind==Defender::stone&&e.jumped!=cell){e.jump=.6666667f;e.jumped=cell;e.animation=0;continue;}
   if(target){if(target->kind==Defender::stone)target->attacking=true;if(!e.attacking)e.cycle=e.animation=0;e.attacking=true;float before=e.cycle;e.cycle+=dt*rate;float period=e.kind==EnemyKind::moth?.55f:e.fox?.67f:e.kind==EnemyKind::snail?1.8f:1.4f;
    if(before<period*.5f&&e.cycle>=period*.5f){target->hp-=e.kind==EnemyKind::snail?22:e.kind==EnemyKind::moth?6:8;target->flash=.15f;if(target->hp<=0){defeat(e.row,cell+.5f,target->kind);*target={};}}if(e.cycle>=period)e.cycle-=period;
   }else{e.attacking=false;e.cycle=0;e.x-=dt*rate*(e.kind==EnemyKind::moth?1.35f:e.fox?.45f:e.kind==EnemyKind::snail?.26f:.7f);}if(e.x<-.6f){outcome=Outcome::lost;return;}
  }
 }
};
}
