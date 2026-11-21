// SPDX-License-Identifier: MIT
#pragma once
#include <cstdint>
namespace ash {
// Track actual outstanding lists. Permission/background transitions can cancel
// lists without a completion; recover that deficit outside active gameplay.
struct RenderPacing {
    std::uint64_t cancelled=0,blockedAt=0;
    bool waiting=false,recoverable=false;
    bool ready(std::uint64_t now,std::uint64_t submitted,std::uint64_t rendered,
               std::uint64_t dropped,bool outsideFight){
        const auto completed=rendered+dropped;
        const auto outstanding=submitted>completed?submitted-completed:0;
        if(cancelled>outstanding)cancelled=outstanding;
        if(outstanding<=cancelled){waiting=false;recoverable=false;return true;}
        if(!waiting||now<blockedAt){waiting=true;blockedAt=now;recoverable=outsideFight;}
        recoverable|=outsideFight;
        if(recoverable&&now-blockedAt>=100000){cancelled=outstanding;waiting=false;recoverable=false;return true;}
        return false;
    }
};
}
