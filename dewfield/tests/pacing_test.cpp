// SPDX-License-Identifier: MIT
#include "../pacing.hpp"
#include <cassert>
#include <cstdio>
#include <algorithm>
static unsigned run(unsigned draw_us, unsigned acquire_us, bool fixed) {
  uint64_t now=1000000,tick=now,present=0,end=now+60000000;
  unsigned frames=0;
  while(now<end) {
    if(now-tick>=33333)tick=now;
    if(now-present>=33333) {
      now+=acquire_us;present=now;now+=draw_us;++frames;
    }
    uint64_t wait=fixed?dewfield_pacing::wait_us(now,tick,present,true):
                       (now<tick+33333?tick+33333-now:0);
    now+=std::max<uint64_t>(1,wait)+5; // bounded dispatch overhead
  }
  return frames;
}
int main() {
  // Realistic draw/acquire costs used to skip every other tick, even though
  // the work is well inside a 33ms budget. No input/release wake is required.
  for(unsigned draw:{1000u,12000u,18000u,28000u})
    for(unsigned acquire:{100u,250u,600u}) {
      auto old=run(draw,acquire,false),fixed=run(draw,acquire,true);
      assert(old<1100&&fixed>1750&&fixed<=1801);
    }
  // Slow frames use available throughput without adding another idle period.
  assert(run(65000,250,true)>900);
  assert(dewfield_pacing::wait_us(1000,0,500,true)==32333);
  assert(dewfield_pacing::wait_us(33334,33333,250,true)==249);
  assert(dewfield_pacing::wait_us(34000,33333,0,true)==32666);
  assert(dewfield_pacing::wait_us(33334,33333,250,false)==33332);
  puts("PASS: 30Hz rendering despite independent tick/acquire phases; idle and backpressure retain bounded waits");
}
