// SPDX-License-Identifier: MIT
#pragma once
#include "model.hpp"
#include <cstddef>
#include <cstring>
#include <span>
#include <type_traits>
namespace iron {
inline constexpr size_t legacy_world_bytes=13380;
static_assert(offsetof(World,extra)==legacy_world_bytes&&sizeof(Entity)==56&&sizeof(Shot)==44,"Keep legacy world prefix stable");
static_assert(std::is_trivially_copyable_v<World>);
struct Career {uint32_t unlocked=0,wins=0;std::array<uint32_t,3> best{};bool easy=false;};
struct Save {uint32_t magic=0x49445736,version=6,checksum=0,has_run=0,generation=0;Career career{};World world{};uint32_t defense_best=0;};
struct LegacySave {uint32_t magic,version,checksum,has_run,generation;Career career;std::array<std::byte,legacy_world_bytes> world;};
static_assert(sizeof(LegacySave)==13424&&offsetof(Save,world)==offsetof(LegacySave,world));
inline uint32_t checksum_bytes(std::span<const std::byte> bytes){uint32_t h=2166136261;for(size_t i=12;i<bytes.size();++i)h=(h^std::to_integer<uint8_t>(bytes[i]))*16777619;return h;}
inline uint32_t checksum(const Save& s){return checksum_bytes(std::as_bytes(std::span{&s,1}));}
inline bool valid_world(const Save& s,bool v4=false){
 if(s.career.unlocked>2||s.has_run>1||s.world.chapter<0||s.world.chapter>2||s.world.job_count<0||s.world.job_count>6||s.world.credits<0||s.world.credits>10000000||!std::isfinite(s.world.time))return false;
 for(auto& a:s.world.e)if(a.kind>supply||a.team>2||!std::isfinite(a.hp)||!std::isfinite(a.p.x)||!std::isfinite(a.p.y)||a.p.x<0||a.p.y<0||a.p.x>28||a.p.y>28||!std::isfinite(a.goal.x)||!std::isfinite(a.goal.y)||!std::isfinite(a.progress))return false;
 for(int i=0;i<s.world.job_count;++i){auto& j=s.world.jobs[i];if(j.kind>=hq||!std::isfinite(j.left)||!std::isfinite(j.total)||j.total<=0)return false;}
 for(auto& shot:s.world.shots)if(shot.alive){if(shot.kind>turret||shot_target(shot)<0||shot_target(shot)>=MAX||!std::isfinite(shot.duration)||shot.duration<=0||!std::isfinite(shot.t)||shot.t<0)return false;if(v4&&(shot.team>1||shot.target>=MAX))return false;}
 for(auto& blast:s.world.blasts)if(blast.alive&&(!std::isfinite(blast.age)||blast.age<0||blast.type>6))return false;
 const auto& x=s.world.extra;if(x.mode>1||x.damage>2||x.armor>2||x.research>2||x.map_rules>1||!std::isfinite(x.research_left)||x.research_left<0||x.research_left>24.01f)return false;
 if(x.research&&(!x.research_left||s.world.tech_level(x.research)>=2))return false;
 if(x.cleared>unsigned(std::max(0,s.world.wave)))return false;
 return true;
}
inline bool valid_save(const Save& s){return s.magic==0x49445736&&s.version==6&&valid_world(s);}
// Decode in place as well as into a separate object, without a large stack copy.
inline bool decode_save(std::span<const std::byte> bytes,Save& output){
 if(bytes.size()<20)return false;uint32_t head[3];std::memcpy(head,bytes.data(),sizeof head);if(head[2]!=checksum_bytes(bytes))return false;
 if(bytes.size()==sizeof(Save)&&head[0]==0x49445736&&head[1]==6){std::memmove(&output,bytes.data(),bytes.size());return valid_save(output);}
 if(bytes.size()!=sizeof(LegacySave)||!((head[0]==0x49445734&&head[1]==4)||(head[0]==0x49445735&&head[1]==5)))return false;
 std::memmove(&output,bytes.data(),bytes.size());output.world.extra={};output.defense_best=0;if(!valid_world(output,head[1]==4))return false;
 output.magic=0x49445736;output.version=6;output.checksum=checksum(output);return true;
}
}
