// SPDX-License-Identifier: MIT
#pragma once
#include "living.hpp"
#include <algorithm>
#include <cstdlib>
namespace farm {
struct FishSpecies {
  const char *cn, *en, *hint_cn, *hint_en;
  uint8_t regions, seasons, weight, min_cm, max_cm;
  uint16_t start, end;
  bool wet_only = false;
};
inline constexpr FishSpecies FishSpeciesList[] = {
  {"银鳞鲫", "Silver crucian", "各处水域 · 四季全天", "Any water / all year", 47,15,12,12,28,360,1560},
  {"溪纹鲦", "Brook dace", "林间 · 春秋白天", "Woods / spring, fall / day", 4,5,7,9,22,360,1080},
  {"暖阳鲈", "Sun perch", "农场湖畔 · 夏秋白天", "Farm, lake / summer, fall / day", 9,6,7,18,42,480,1080},
  {"青背鲤", "Green carp", "农场果园 · 四季白天", "Farm, orchard / all year / day", 33,15,8,22,50,360,1140},
  {"雨鳍鳟", "Rain trout", "林间湖畔 · 春夏秋雨天", "Woods, lake / wet spring-fall", 12,7,5,20,44,360,1560,true},
  {"暮影鲶", "Dusk catfish", "林间湖畔 · 四季傍晚后", "Woods, lake / after 18:00", 12,15,5,26,60,1080,1560},
  {"霜花银鱼", "Frost smelt", "林间湖畔 · 冬季全天", "Woods, lake / winter", 12,8,6,8,20,360,1560},
  {"星尾锦鲤", "Star koi", "果园湖畔 · 春秋傍晚后", "Orchard, lake / spring-fall dusk", 40,5,2,30,72,1080,1560}
};
inline constexpr int FishSpeciesCount = 8;
inline bool fish_available(const State &s, int id) {
  if(id<0 || id>=FishSpeciesCount || indoors(s.region))return false;
  const auto &f=FishSpeciesList[id];
  return (f.regions&(1u<<s.region)) && (f.seasons&(1u<<season_of(s.day))) &&
    s.minute>=f.start && s.minute<f.end && (!f.wet_only || wet_weather(Weather(s.weather)));
}
inline int choose_fish(State &s) {
  unsigned total=0;
  for(int i=0;i<FishSpeciesCount;++i)if(fish_available(s,i))total+=FishSpeciesList[i].weight;
  if(!total)return -1;
  unsigned pick=random(s)%total;
  for(int i=0;i<FishSpeciesCount;++i)if(fish_available(s,i)) {
    if(pick<FishSpeciesList[i].weight)return i;
    pick-=FishSpeciesList[i].weight;
  }
  return -1;
}
inline int fishing_level(const State &s) {
  return s.fish_caught>=60?3:s.fish_caught>=25?2:s.fish_caught>=8?1:0;
}
struct FishingSession {
  enum class Phase { Waiting, Bite, Reeling, Caught, Escaped };
  Phase phase=Phase::Waiting;
  uint32_t elapsed=0, bite_at=0, cycle_ms=2400;
  int species=-1, length=0, attempts=2, count=0, center=500, radius=185;
  bool perfect=false, discovered=false, record=false;
  void begin(State &s) {
    *this={};species=choose_fish(s);
    if(species<0){phase=Phase::Escaped;return;}
    const auto &f=FishSpeciesList[species];
    bite_at=1200+random(s)%1600;
    length=f.min_cm+random(s)%(f.max_cm-f.min_cm+1);
    center=370+random(s)%260;
    cycle_ms=f.weight<=5?2000:2400;
    radius=(f.weight<=5?145:185)+fishing_level(s)*18;
  }
  int cursor() const {
    unsigned t=elapsed%cycle_ms;
    return int((t<=cycle_ms/2?t:cycle_ms-t)*2000/cycle_ms);
  }
  void tick(uint32_t ms) {
    if(phase==Phase::Caught || phase==Phase::Escaped)return;
    elapsed=std::min(12000u,elapsed+std::min(ms,1000u));
    if(phase==Phase::Waiting && elapsed>=bite_at)phase=Phase::Bite;
    if((phase==Phase::Bite && elapsed>bite_at+1400) ||
       (phase==Phase::Reeling && elapsed>9000))phase=Phase::Escaped;
  }
  bool press(State &s) {
    if(phase==Phase::Bite) {phase=Phase::Reeling;elapsed=0;return false;}
    // Early taps leave the float in the water; terminal presses never award twice.
    if(phase!=Phase::Reeling)return false;
    int error=std::abs(cursor()-center);
    if(error>radius) {if(--attempts==0)phase=Phase::Escaped;return false;}
    if(s.fish>=999){phase=Phase::Escaped;return false;}
    perfect=error<=55;
    count=std::min(perfect?2:1,999-int(s.fish));
    if(!change_item(s,16,count)){phase=Phase::Escaped;return false;}
    discovered=!s.fish_records[species];record=length>s.fish_records[species];
    s.fish_records[species]=std::max(int(s.fish_records[species]),length);
    s.fish_caught=uint16_t(std::min(65535,int(s.fish_caught)+1));
    if(perfect)s.perfect_catches=uint16_t(std::min(65535,int(s.perfect_catches)+1));
    phase=Phase::Caught;sync_inventory(s);return true;
  }
};
} // namespace farm
