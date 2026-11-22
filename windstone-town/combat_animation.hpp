#pragma once

#include "game.hpp"

#include <algorithm>

namespace wind {

struct CombatAnimation {
    static constexpr unsigned Strike=100, Counter=290, Victory=240, End=470;
    unsigned age=End;
    int guardian_loss=0, target=-1, ally_loss=0;

    void record(int guardian_before,const std::array<uint8_t,HeroCount>& party_before,const State& s) {
        age=0;
        guardian_loss=guardian_before-s.trip.hp;
        target=-1;
        ally_loss=0;
        for (int id=0;id<HeroCount;++id) {
            if (!(s.trip.team&(1u<<id)) || party_before[id]<=s.heroes[id].hp) continue;
            target=id;
            ally_loss=party_before[id]-s.heroes[id].hp;
            break;
        }
    }
    void step() {age=std::min(End,age+SimulationBeatMs);}
    bool finishing(const Expedition& trip,unsigned time) const {
        return trip.result==1 && guardian_loss>0 && time<Victory;
    }
    int guardian_hp(const Expedition& trip,unsigned time) const {
        return trip.hp+(time<Strike?guardian_loss:0);
    }
    int ally_hp(const Hero& hero,int id,unsigned time) const {
        return std::min(100,hero.hp+(id==target && time<Counter?ally_loss:0));
    }
    int ally_pose(unsigned time) const {
        if (!guardian_loss) return 0;
        return time<Strike?1:time<180?2:time<235?3:0;
    }
    int guardian_pose(unsigned time) const {
        if (guardian_loss && time>=Strike && time<205) return 1;
        if (target<0 || time<220 || time>=End) return 0;
        return time<Counter?2:time<365?3:4;
    }
};

}
