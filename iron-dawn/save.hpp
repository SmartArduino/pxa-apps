// SPDX-License-Identifier: MIT
#pragma once
#include "model.hpp"
namespace iron {
static_assert(sizeof(World)==13380&&sizeof(Entity)==56&&sizeof(Shot)==44,"Save v4/v5 layout must stay compatible");
struct Career {uint32_t unlocked=0,wins=0;std::array<uint32_t,3> best{};bool easy=false;};
struct Save {uint32_t magic=0x49445735,version=5,checksum=0,has_run=0,generation=0;Career career{};World world{};};
inline uint32_t checksum(const Save& s){auto p=reinterpret_cast<const uint8_t*>(&s);uint32_t h=2166136261;for(size_t i=12;i<sizeof s;++i)h=(h^p[i])*16777619;return h;}
inline bool valid_save(const Save& s){
 if(!((s.magic==0x49445734&&s.version==4)||(s.magic==0x49445735&&s.version==5)))return false;
 if(s.career.unlocked>2||s.has_run>1||s.world.chapter<0||s.world.chapter>2||s.world.job_count<0||s.world.job_count>6||s.world.credits<0||s.world.credits>10000000||!std::isfinite(s.world.time))return false;
 for(auto& a:s.world.e)if(a.kind>supply||a.team>2||!std::isfinite(a.hp)||!std::isfinite(a.p.x)||!std::isfinite(a.p.y)||a.p.x<0||a.p.y<0||a.p.x>28||a.p.y>28||!std::isfinite(a.goal.x)||!std::isfinite(a.goal.y)||!std::isfinite(a.progress))return false;
 for(int i=0;i<s.world.job_count;++i){auto& j=s.world.jobs[i];if(j.kind>=hq||!std::isfinite(j.left)||!std::isfinite(j.total)||j.total<=0)return false;}
 for(auto& shot:s.world.shots)if(shot.alive){if(shot.kind>turret||shot_target(shot)<0||shot_target(shot)>=MAX||!std::isfinite(shot.duration)||shot.duration<=0||!std::isfinite(shot.t)||shot.t<0)return false;if(s.version==4&&(shot.team>1||shot.target>=MAX))return false;}
 for(auto& blast:s.world.blasts)if(blast.alive&&(!std::isfinite(blast.age)||blast.age<0||blast.type>6))return false;
 return true;
}
}
