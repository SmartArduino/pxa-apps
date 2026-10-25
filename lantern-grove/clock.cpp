// SPDX-License-Identifier: CC0-1.0
#include "core.hpp"
#include <time.h>
namespace grove {
uint64_t wall_seconds() {
    timespec t{};
    if(clock_gettime(CLOCK_REALTIME,&t)!=0||t.tv_sec<0) return 0;
    const uint64_t seconds=uint64_t(t.tv_sec);
    return valid_wall(seconds)?seconds:0;
}
uint64_t monotonic_ms() {
    timespec t{};
    if(clock_gettime(CLOCK_MONOTONIC,&t)!=0||t.tv_sec<0) return 0;
    return uint64_t(t.tv_sec)*1000+uint64_t(t.tv_nsec)/1000000;
}
}
