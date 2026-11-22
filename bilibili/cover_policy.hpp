// SPDX-License-Identifier: MIT
#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <string_view>
namespace bili {
struct CoverPolicy {
    uint64_t next=0;
    std::array<uint64_t,20> admitted{};
    unsigned count=0,cursor=0,failures=0;
    void defer(uint64_t until){next=std::max(next,until);}
    bool admit(uint64_t now) {
        if(now<next)return false;
        if(count>=20 && now-admitted[cursor]<60000){defer(admitted[cursor]+60000);return false;}
        admitted[cursor]=now;cursor=(cursor+1)%admitted.size();count=std::min(20u,count+1);
        next=now+1000;return true;
    }
    uint64_t fail(uint64_t now,bool network) {
        uint64_t delay=std::min<uint64_t>(300000,5000ull<<std::min(6u,failures));
        failures=std::min(6u,failures+1);if(network)defer(now+delay);return now+delay;
    }
    static uint64_t retry_after(std::string_view value,uint64_t minimum) {
        // Delta-seconds are honored without shortening a server's long delay.
        // HTTP dates or malformed values get a conservative five-minute wait.
        uint64_t seconds=0;if(value.empty())return std::max<uint64_t>(minimum,300000);
        for(char c:value){if(c<'0'||c>'9'||seconds>(UINT32_MAX-unsigned(c-'0'))/10)return std::max<uint64_t>(minimum,300000);seconds=seconds*10+c-'0';}
        return std::max(minimum,seconds*1000);
    }
};
}
