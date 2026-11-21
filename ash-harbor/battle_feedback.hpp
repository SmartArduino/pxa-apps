// SPDX-License-Identifier: MIT
#pragma once
#include "combat.hpp"
#include <string_view>

namespace ash {
inline const char* directionGlyph(uint32_t mask){
    int horizontal=int(bool(mask&Right))-int(bool(mask&Left));
    int vertical=int(bool(mask&Up))-int(bool(mask&Down));
    constexpr const char* glyphs[]={"↙","↓","↘","←","","→","↖","↑","↗"};
    return glyphs[4+horizontal+vertical*3];
}
inline bool localRoundWon(const Arena& arena,unsigned player){return arena.winner==int(player);}
inline bool localMatchWon(const Arena& arena,unsigned player){return arena.wins[player]>=2;}
// This is presentation state, observed on either the simulation or snapshots.
// Keeping both actors here also works with the existing network wire format.
struct BattleFeedback {
    std::array<Action,2> previous{Action::idle,Action::idle},last{Action::idle,Action::idle};
    std::array<int,2> ages{};
    std::array<uint32_t,2> began{};
    std::array<bool,2> valid{};
    uint32_t tick=0;
    void observe(const Arena& arena){
        if(arena.tick<tick)*this={};
        tick=arena.tick;
        for(unsigned i=0;i<2;++i){const auto& f=arena.f[i];
            if(attack(f.action)&&(f.action!=previous[i]||f.age<ages[i])){
                last[i]=f.action;began[i]=arena.tick;valid[i]=true;
            }
            previous[i]=f.action;ages[i]=f.age;
        }
    }
    const char* moveName(const Arena& arena,unsigned player)const{
        if(!valid[player]||arena.tick-began[player]>=70)return "";
        auto a=last[player];int index=a==Action::fire?0:a==Action::rise?1:a==Action::rush?2:a==Action::super?3:-1;
        return index<0?"":roster[arena.f[player].character].arts[index];
    }
};
} // namespace ash
