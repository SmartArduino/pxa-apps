// SPDX-License-Identifier: MIT
#pragma once
#include "model.hpp"
#include <cstddef>
#include <cstring>
#include <span>
#include <type_traits>
namespace iron {
inline constexpr size_t legacy_world_bytes=13380;
// Frozen disk layouts from releases 1.0–1.6. Their grid stride stays 28.
struct LegacyCareer {uint32_t unlocked=0,wins=0;std::array<uint32_t,3> best{};bool easy=false;};
struct LegacyWorld {
 std::array<Entity,MAX> e{};std::array<Shot,64> shots{};std::array<Blast,48> blasts{};
 std::array<uint8_t,28*28> terrain{},ore{},fog{},seen{},obstacles{};std::array<Job,6> jobs{};
 uint32_t rng=83711;int chapter=0,credits=1100,power_net=0,job_count=0,wave=0,killed=0,lost=0,earned=0;
 float time=0,next_wave=58,strike_cd=0,capture=0,autosave=0,notice_time=0,reveal_timer=0,radio_time=0;
 bool won=false,defeated=false,story_beat=false;char notice[128]{};unsigned sound=0;Expansion extra{};
};
struct LegacySave {uint32_t magic,version,checksum,has_run,generation;LegacyCareer career;std::array<std::byte,legacy_world_bytes> world;};
struct LegacySave6 {uint32_t magic,version,checksum,has_run,generation;LegacyCareer career;LegacyWorld world;uint32_t defense_best;};
static_assert(sizeof(LegacySave)==13424&&sizeof(LegacySave6)==13448&&offsetof(LegacyWorld,extra)==legacy_world_bytes);
static_assert(sizeof(Job)==12&&offsetof(Job,left)==4);
static_assert(sizeof(Entity)==56&&sizeof(Shot)==44&&offsetof(World,terrain)==offsetof(LegacyWorld,terrain));
struct Career {uint32_t unlocked=0,wins=0;std::array<uint32_t,CHAPTERS> best{};bool easy=false;};
inline constexpr uint32_t save_magic=0x49445739,save_version=9;
struct Save {uint32_t magic=save_magic,version=save_version,checksum=0,has_run=0,generation=0;Career career{};World world{};uint32_t defense_best=0;};
static_assert(std::is_trivially_copyable_v<Save>);
inline uint32_t checksum_bytes(std::span<const std::byte> bytes){uint32_t h=2166136261;for(size_t i=12;i<bytes.size();++i)h=(h^std::to_integer<uint8_t>(bytes[i]))*16777619;return h;}
inline uint32_t checksum(const Save& s){return checksum_bytes(std::as_bytes(std::span{&s,1}));}
inline bool valid_world(const Save& s,bool v4=false){
 if(s.career.unlocked>=CHAPTERS||s.has_run>1||s.world.chapter<0||s.world.chapter>=CHAPTERS||s.world.side<28||s.world.side>N||s.world.job_count<0||s.world.job_count>6||s.world.credits<0||s.world.credits>10000000||!std::isfinite(s.world.time))return false;
 for(auto& a:s.world.e)if(a.kind>=kind_count||a.team>2||!std::isfinite(a.hp)||!std::isfinite(a.p.x)||!std::isfinite(a.p.y)||a.p.x<0||a.p.y<0||a.p.x>s.world.side||a.p.y>s.world.side||!std::isfinite(a.goal.x)||!std::isfinite(a.goal.y)||!std::isfinite(a.progress))return false;
 for(const auto& a:s.world.e){
  int escort=escort_id(a);
  if(escort>=MAX|| (a.hp>0&&(escort>=0||a.target==retreat_order)&&(a.team!=0||!combat_kind(a.kind))))return false;
 }
 for(int i=0;i<s.world.job_count;++i){auto& j=s.world.jobs[i];if(!unit_kind(j.kind)||j.producer>MAX||!std::isfinite(j.left)||!std::isfinite(j.total)||j.total<=0)return false;}
 const auto& build=s.world.construction;
 if(build.kind&&(!s.world.build_active()||!std::isfinite(build.left)||!std::isfinite(build.total)
    ||build.left<0||build.total<=0||build.left>build.total))return false;
 for(auto& shot:s.world.shots)if(shot.alive){if((!combat_kind(shot.kind)&&!defence_kind(shot.kind))||shot_target(shot)<0||shot_target(shot)>=MAX||!std::isfinite(shot.duration)||shot.duration<=0||!std::isfinite(shot.t)||shot.t<0)return false;if(v4&&(shot.team>1||shot.target>=MAX))return false;}
 for(auto& blast:s.world.blasts)if(blast.alive&&(!std::isfinite(blast.age)||blast.age<0||blast.type>6))return false;
 const auto& x=s.world.extra;if(x.mode>1||x.damage>2||x.armor>2||x.research>2||x.map_rules>1||!std::isfinite(x.research_left)||x.research_left<0||x.research_left>24.01f)return false;
 if(x.research&&(!x.research_left||s.world.tech_level(x.research)>=2))return false;
 if(x.cleared>unsigned(std::max(0,s.world.wave)))return false;
 auto& m=s.world.mission;if(m.stage>3||m.delivered>7||!std::isfinite(m.support_left)||m.support_left<0)return false;
 for(int i=0;i<3;++i)if(m.convoy[i]<-1||m.convoy[i]>=MAX||m.waypoint[i]>3)return false;
 for(uint8_t squad:s.world.squads)if(squad>squad_count)return false;
 for(int c=0;c<N*N;++c)if(s.world.terrain[c]>railbed)return false;
 return true;
}
inline bool valid_save(const Save& s){return s.magic==save_magic&&s.version==save_version&&valid_world(s);}
// Version 7 has the same prefix/suffix and five grids at a 48-byte stride.
inline bool migrate_save7(std::span<const std::byte> bytes,Save& output){
 constexpr int old_side=48,grids=5;
 constexpr size_t prefix=offsetof(Save,world)+offsetof(World,terrain);
 constexpr size_t tail=offsetof(Save,world)+offsetof(World,jobs);
 constexpr size_t old_tail=prefix+grids*old_side*old_side;
 static_assert(tail==prefix+grids*N*N && sizeof(Save)-tail-sizeof(Job)-MAX==21092-old_tail);
 auto dest=std::as_writable_bytes(std::span{&output,1});
 // Work backwards so decoding into the same buffer leaves unread rows intact.
 uint32_t defense_best=0;
 std::memcpy(&defense_best,bytes.data()+bytes.size()-4,4);
 std::memmove(dest.data()+tail,bytes.data()+old_tail,bytes.size()-old_tail-4);
 output.defense_best=defense_best;
 output.world.construction={};
 output.world.squads.fill(0);
 for(auto& job:output.world.jobs)job.producer=0;
 for(int grid=grids-1;grid>=0;--grid)
  for(int row=old_side-1;row>=0;--row){
   size_t src=prefix+grid*old_side*old_side+row*old_side,dst=prefix+grid*N*N+row*N;
   std::memmove(dest.data()+dst,bytes.data()+src,old_side);
  }
 std::memmove(dest.data(),bytes.data(),prefix);
 for(int grid=0;grid<grids;++grid)
  for(int row=0;row<N;++row){
   unsigned padding=grid==0?water:grid==4?1:0;
   size_t dst=prefix+grid*N*N+row*N;
   int first=row<old_side?old_side:0;
   std::memset(dest.data()+dst+first,padding,N-first);
  }
 if(output.world.side>old_side||!valid_world(output))return false;
 output.magic=save_magic;output.version=save_version;output.checksum=checksum(output);
 return true;
}
inline bool migrate_save8(std::span<const std::byte> bytes,Save& output){
 constexpr size_t added=offsetof(World,squads)+offsetof(Save,world);
 static_assert(added+4==30064 && offsetof(Save,defense_best)==added+MAX);
 uint32_t defense=0;
 std::memcpy(&defense,bytes.data()+added,4);
 std::memmove(&output,bytes.data(),added);
 output.world.squads.fill(0);
 output.defense_best=defense;
 if(!valid_world(output))return false;
 output.magic=save_magic;output.version=save_version;output.checksum=checksum(output);
 return true;
}
inline bool decode_save(std::span<const std::byte> bytes,Save& output){
 if(bytes.size()<20)return false;uint32_t head[5];std::memcpy(head,bytes.data(),sizeof head);if(head[2]!=checksum_bytes(bytes))return false;
 if(bytes.size()==sizeof(Save)&&head[0]==save_magic&&head[1]==save_version){std::memmove(&output,bytes.data(),bytes.size());return valid_save(output);}
 if(bytes.size()==30064&&head[0]==0x49445738&&head[1]==8)return migrate_save8(bytes,output);
 if(bytes.size()==21092&&head[0]==0x49445737&&head[1]==7)return migrate_save7(bytes,output);
 bool old=head[1]==4||head[1]==5;if((old&&(bytes.size()!=sizeof(LegacySave)||head[0]!=(head[1]==4?0x49445734u:0x49445735u)))||(!old&&(bytes.size()!=sizeof(LegacySave6)||head[0]!=0x49445736||head[1]!=6)))return false;
 LegacyWorld world{};LegacyCareer career;uint32_t defense=0;std::memcpy(&career,bytes.data()+20,sizeof career);std::memcpy(&world,bytes.data()+44,old?legacy_world_bytes:sizeof world);if(!old)std::memcpy(&defense,bytes.data()+sizeof(LegacySave6)-4,4);
 if(career.unlocked>2||world.chapter<0||world.chapter>2)return false;
 // Copy source before clearing destination: initialization decodes in place.
 std::memset(&output,0,sizeof output);output.magic=save_magic;output.version=save_version;output.has_run=head[3];output.generation=head[4];output.defense_best=defense;output.career.unlocked=(career.wins&4)?3:career.unlocked;output.career.wins=career.wins;output.career.easy=career.easy;std::copy(career.best.begin(),career.best.end(),output.career.best.begin());
 auto& w=output.world;std::memcpy(&w,&world,offsetof(LegacyWorld,terrain));w.terrain.fill(water);w.obstacles.fill(1);
 for(int y=0;y<28;++y)for(int x=0;x<28;++x){int a=y*28+x,b=y*N+x;w.terrain[b]=world.terrain[a];w.ore[b]=world.ore[a];w.fog[b]=world.fog[a];w.seen[b]=world.seen[a];w.obstacles[b]=world.obstacles[a];}
 std::memcpy(&w.jobs,&world.jobs,sizeof(LegacyWorld)-offsetof(LegacyWorld,jobs));w.side=28;w.mission=Mission{};w.route_credits=6;for(auto& job:w.jobs)job.producer=0;
 if(!valid_world(output,head[1]==4))return false;output.checksum=checksum(output);return true;
}
}
