// SPDX-License-Identifier: MIT
#pragma once
#include <cstdint>
namespace starsoil {
enum class NoticeKind : uint8_t { Info, Warning, Milestone, Dialogue };
}
namespace starsoil_notices {
inline bool active(uint32_t now,uint32_t until) {return until && int32_t(until-now)>0;}
constexpr unsigned duration(starsoil::NoticeKind kind) {
  using starsoil::NoticeKind;
  return kind==NoticeKind::Info?1600:kind==NoticeKind::Warning?2000:kind==NoticeKind::Milestone?4000:4500;
}
constexpr unsigned priority(starsoil::NoticeKind kind) {
  using starsoil::NoticeKind;
  return kind==NoticeKind::Info?1:kind==NoticeKind::Warning?2:3;
}
struct Gate {
  uint32_t key=0,shown_at=0;
  starsoil::NoticeKind kind=starsoil::NoticeKind::Info;
  bool initialized=false;
  bool accept(const char *text,starsoil::NoticeKind next,uint32_t now,bool showing) {
    if(!text || !*text)return false;
    uint32_t hash=2166136261u;
    for(const auto *p=reinterpret_cast<const unsigned char *>(text);*p;++p)hash=(hash^*p)*16777619u;
    hash=(hash^uint32_t(next))*16777619u;
    if(initialized && hash==key && uint32_t(now-shown_at)<duration(kind)+800)return false;
    if(showing && priority(next)<priority(kind))return false;
    key=hash;shown_at=now;kind=next;initialized=true;return true;
  }
};
} // namespace starsoil_notices
